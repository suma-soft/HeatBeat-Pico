// hb_http.c
// Minimalny, jednorazowy klient TCP oparty na lwIP raw API.
// Obsługuje GET /device/{id}/settings oraz POST /device/{id}/reading.

#include "hb_http.h"
#include "hb_proto.h"

#ifndef ENABLE_WIFI
#ifdef TEMP_DISABLE_WIFI
#if TEMP_DISABLE_WIFI
#define ENABLE_WIFI 0
#else
#define ENABLE_WIFI 1
#endif
#else
#define ENABLE_WIFI 1
#endif
#endif

#if ENABLE_WIFI
#include "pico/cyw43_arch.h"
#include "lwip/ip4_addr.h"
#include "lwip/tcp.h"
#endif

#include "pico/time.h"
#include <string.h>
#include <stdio.h>

#ifndef HB_HTTP_RECV_BUF_MAX
#define HB_HTTP_RECV_BUF_MAX 1024
#endif

#if ENABLE_WIFI

typedef enum {
    ST_IDLE = 0,
    ST_CONNECTING,
    ST_SENDING,
    ST_RECEIVING,
    ST_DONE,
    ST_ERROR
} hb_state_t;

typedef struct {
    struct tcp_pcb *pcb;
    hb_state_t state;
    int err;                    // hb_http_status_t
    const char *req;            // wskaźnik na bufor requestu
    uint32_t req_len;
    uint32_t req_sent;

    char resp[HB_HTTP_RECV_BUF_MAX];
    uint32_t resp_len;
} hb_client_t;

static err_t on_connected(void *arg, struct tcp_pcb *tpcb, err_t lwip_err);
static err_t on_sent(void *arg, struct tcp_pcb *tpcb, u16_t len);
static err_t on_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err);
static void  on_err(void *arg, err_t lwip_err);

static err_t on_connected(void *arg, struct tcp_pcb *tpcb, err_t lwip_err) {
    hb_client_t *c = (hb_client_t *)arg;
    printf("[HTTP] Połączono: err=%d\n", lwip_err);
    
    if (!c || !tpcb) {
        printf("[HTTP] KRYTYCZNY: on_connected z NULL arg lub tpcb\n");
        return ERR_ARG;
    }
    
    if (lwip_err != ERR_OK) {
        printf("[HTTP] Błąd połączenia lwIP: %d\n", lwip_err);
        c->err = HB_HTTP_ERR_CONNECT;
        c->state = ST_ERROR;
        return lwip_err;
    }
    
    // Sprawdź czy jesteśmy w oczekiwanym stanie
    if (c->state != ST_CONNECTING) {
        printf("[HTTP] OSTRZEŻENIE: Połączono w nieoczekiwanym stanie: %d\n", c->state);
    }
    tcp_arg(tpcb, c);
    tcp_err(tpcb, on_err);
    tcp_recv(tpcb, NULL);
    tcp_sent(tpcb, on_sent);    // KLUCZOWE

    c->state = ST_SENDING;
    c->pcb = tpcb;

    printf("[HTTP] Przełączenie do wysyłania\n");

    // natychmiast wypchnij pierwszą porcję
    u16_t space = tcp_sndbuf(tpcb);
    if (space > 0 && c->req_sent < c->req_len) {
        u16_t chunk = (u16_t)(c->req_len - c->req_sent);
        if (chunk > space) chunk = space;
        printf("[HTTP] Wysyłam %u bajtów\n", chunk);
        err_t w = tcp_write(tpcb, c->req + c->req_sent, chunk, TCP_WRITE_FLAG_COPY);
        if (w == ERR_OK) tcp_output(tpcb);
    }
    return ERR_OK;
}

static err_t on_sent(void *arg, struct tcp_pcb *tpcb, u16_t len) {
    hb_client_t *c = (hb_client_t *)arg;
    printf("[HTTP] Wysłano %u bajtów\n", len);
    c->req_sent += len;

    if (c->req_sent >= c->req_len) {
        printf("[HTTP] Wszystkie dane wysłane, przełączenie do odbierania\n");
        c->state = ST_RECEIVING;
        tcp_recv(tpcb, on_recv);
    } else {
        printf("[HTTP] Pozostało do wysłania (%u/%u)\n", c->req_sent, c->req_len);
        u16_t space = tcp_sndbuf(tpcb);
        if (space > 0) {
            u16_t chunk = (u16_t)(c->req_len - c->req_sent);
            if (chunk > space) chunk = space;
            printf("[HTTP] Wysyłam kolejny fragment %u bajtów\n", chunk);
            err_t w = tcp_write(tpcb, c->req + c->req_sent, chunk, TCP_WRITE_FLAG_COPY);
            if (w == ERR_OK) tcp_output(tpcb);
            else if (w != ERR_MEM) {
                printf("[HTTP] Błąd tcp_write: %d\n", w);
                c->err = HB_HTTP_ERR_SEND;
                c->state = ST_ERROR;
                return w;
            }
        }
    }
    return ERR_OK;
}

static err_t on_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err) {
    hb_client_t *c = (hb_client_t *)arg;

    printf("[HTTP] on_recv: p=%p, err=%d, resp_len=%u\n", p, err, c->resp_len);

    if (!p) {
        printf("[HTTP] Połączenie zamknięte, gotowe\n");
        c->state = ST_DONE;
        return ERR_OK;
    }
    if (err != ERR_OK) {
        printf("[HTTP] Błąd odbierania: %d\n", err);
        pbuf_free(p);
        c->err = HB_HTTP_ERR_RECV;
        c->state = ST_ERROR;
        return err;
    }

    printf("[HTTP] Odebrano %u bajtów\n", p->tot_len);

    struct pbuf *q = p;
    while (q && c->resp_len < sizeof(c->resp)) {
        u16_t copy = q->len;
        if (copy > (u16_t)(sizeof(c->resp) - c->resp_len))
            copy = (u16_t)(sizeof(c->resp) - c->resp_len);
        memcpy(c->resp + c->resp_len, q->payload, copy);
        c->resp_len += copy;
        q = q->next;
    }
    
    // Pokazujmy tylko kod odpowiedzi, nie całą treść
    if (c->resp_len >= 12) {  // "HTTP/1.1 200"
        char status_line[64];
        int copy_len = c->resp_len < 63 ? c->resp_len : 63;
        memcpy(status_line, c->resp, copy_len);
        status_line[copy_len] = '\0';
        // Znajdź koniec pierwszej linii
        char *end = strchr(status_line, '\r');
        if (end) *end = '\0';
        end = strchr(status_line, '\n');
        if (end) *end = '\0';
        printf("[HTTP] Status: %s\n", status_line);
    }
    
    tcp_recved(tpcb, p->tot_len);
    pbuf_free(p);
    return ERR_OK;
}

static void on_err(void *arg, err_t lwip_err) {
    hb_client_t *c = (hb_client_t *)arg;
    printf("[HTTP] Błąd lwIP: %d (stan=%d)\n", lwip_err, c ? c->state : -1);
    
    if (!c) {
        printf("[HTTP] KRYTYCZNY: on_err wywołany z NULL arg\n");
        return;
    }
    
    // LwIP błąd -14 to ERR_WOULDBLOCK - nie krytyczny
    if (lwip_err == -14) {
        printf("[HTTP] ERR_WOULDBLOCK - nie krytyczny, kontynuujemy\n");
        return; // Nie zmieniaj stanu na ERROR
    }
    
    if (c->state != ST_DONE) {
        c->err = HB_HTTP_ERR_CONNECT;
        c->state = ST_ERROR;
        // Wyzeruj PCB ponieważ lwIP już go usunął
        c->pcb = NULL;
    }
}

static const char* find_http_body(const char *buf, size_t len) {
    for (size_t i = 0; i + 3 < len; ++i) {
        if (buf[i] == '\r' && buf[i+1] == '\n' && buf[i+2] == '\r' && buf[i+3] == '\n')
            return buf + i + 4;
    }
    return NULL;
}

static hb_http_status_t run_client(hb_client_t *c, const ip4_addr_t *ip, u16_t port, uint32_t timeout_ms) {
    printf("[HTTP] Start klienta (timeout=%ums)\n", (unsigned)timeout_ms);
    
    c->pcb = tcp_new_ip_type(IPADDR_TYPE_V4);
    if (!c->pcb) {
        printf("[HTTP] tcp_new nie powiódł się\n");
        return HB_HTTP_ERR_CONNECT;
    }

    tcp_arg(c->pcb, c);
    tcp_err(c->pcb, on_err);

    c->state = ST_CONNECTING;
    printf("[HTTP] Łączenie...\n");
    err_t e = tcp_connect(c->pcb, ip, port, on_connected);
    if (e != ERR_OK) {
        printf("[HTTP] tcp_connect nie powiódł się: %d\n", e);
        tcp_close(c->pcb);
        c->pcb = NULL;
        return HB_HTTP_ERR_CONNECT;
    }

    absolute_time_t deadline = make_timeout_time_ms(timeout_ms);
    absolute_time_t last_progress = get_absolute_time();
    uint32_t loop_count = 0;
    hb_state_t last_state = c->state;
    
    printf("[HTTP] Pętla oczekiwania rozpoczęta, stan=%d\n", c->state);
    
    while (c->state != ST_DONE && c->state != ST_ERROR) {
        loop_count++;
        absolute_time_t now = get_absolute_time();
        
        // BACKUP TIMEOUT: 1 milion iteracji (~1-2 sekundy)
        if (loop_count > 1000000) {
            printf("[HTTP] EMERGENCY TIMEOUT po %u iteracjach (stan=%d)\n", (unsigned)loop_count, c->state);
            c->err = HB_HTTP_ERR_TIMEOUT;
            c->state = ST_ERROR;
            break;
        }
        
        // Emergency timeout - 2x dłuższy niż normalny
        if (absolute_time_diff_us(now, deadline) <= 0) {
            printf("[HTTP] TIMEOUT po %u iteracjach (stan=%d)\n", (unsigned)loop_count, c->state);
            c->err = HB_HTTP_ERR_TIMEOUT;
            c->state = ST_ERROR;
            break;
        }
        
        // Sprawdź czy stan się zmienił (postęp)
        if (c->state != last_state) {
            printf("[HTTP] Zmiana stanu: %d -> %d\n", last_state, c->state);
            last_state = c->state;
            last_progress = now;
        }
        
        // Timeout dla braku postępu - agresywniejszy dla CONNECTING
        uint32_t progress_timeout = (c->state == ST_CONNECTING) ? 2000000 : 4000000; // 2s vs 4s
        if (absolute_time_diff_us(now, last_progress) > progress_timeout) {
            printf("[HTTP] Brak postępu przez %us, przerwanie (stan=%d)\n", 
                   progress_timeout / 1000000, c->state);
            c->err = HB_HTTP_ERR_TIMEOUT;
            c->state = ST_ERROR;
            break;
        }
        
        cyw43_arch_poll();
        // Brak delay - maksymalna szybkość pętli
        
        // Częstsze logowanie co 100k iteracji
        if (loop_count % 100000 == 0) {
            printf("[HTTP] Loop %u: stan=%d, req_sent=%u/%u\n", 
                   (unsigned)loop_count, c->state, c->req_sent, c->req_len);
        }

        if (c->state == ST_SENDING && c->req_sent < c->req_len) {
            // Sprawdź czy PCB jest nadal ważny
            if (!c->pcb) {
                printf("[HTTP] PCB został zamknięty podczas wysyłania\n");
                c->err = HB_HTTP_ERR_CONNECT;
                c->state = ST_ERROR;
                break;
            }
            
            u16_t space = tcp_sndbuf(c->pcb);
            if (space > 0) {
                u16_t chunk = (u16_t)(c->req_len - c->req_sent);
                if (chunk > space) chunk = space;
                err_t w = tcp_write(c->pcb, c->req + c->req_sent, chunk, TCP_WRITE_FLAG_COPY);
                if (w == ERR_OK) {
                    tcp_output(c->pcb);
                } else if (w != ERR_MEM) {
                    printf("[HTTP] Błąd tcp_write: %d\n", w);
                    c->err = HB_HTTP_ERR_SEND;
                    c->state = ST_ERROR;
                }
            }
        }
    }

    // Uproszczony cleanup PCB bez delay
    if (c->pcb) {
        printf("[HTTP] Cleanup PCB - reset callbacków\n");
        tcp_arg(c->pcb, NULL);
        tcp_recv(c->pcb, NULL);
        tcp_sent(c->pcb, NULL);
        tcp_err(c->pcb, NULL);
        
        tcp_close(c->pcb);
        c->pcb = NULL;
        
        printf("[HTTP] PCB zamknięty\n");
    }
    
    printf("[HTTP] Klient HTTP zakończony (stan=%d)\n", c->state);
    return (c->state == ST_DONE) ? HB_HTTP_OK : (hb_http_status_t)c->err;
}

// ───────────────────────── GET /settings ─────────────────────────

hb_http_status_t hb_http_get_settings(
    const char *host,
    uint16_t port,
    int device_id,
    hb_settings_response_t *out_settings,
    uint32_t timeout_ms)
{
    if (!host || !out_settings) return HB_HTTP_ERR_PARAM;

    ip4_addr_t ip;
    if (!ip4addr_aton(host, &ip)) return HB_HTTP_ERR_IP;

    char req[256];
    int n = hb_build_http_get_settings(req, sizeof(req), host, port, device_id);
    if (n < 0) return HB_HTTP_ERR_PARAM;

    hb_client_t c = {
        .pcb = NULL,
        .state = ST_IDLE,
        .err = 0,
        .req = req,
        .req_len = (uint32_t)n,
        .req_sent = 0,
        .resp_len = 0,
    };

    hb_http_status_t st = run_client(&c, &ip, port, timeout_ms);
    if (st != HB_HTTP_OK) return st;

    const char *body = find_http_body(c.resp, c.resp_len);
    if (!body) return HB_HTTP_ERR_PARSE;

    if (c.resp_len < (sizeof(c.resp) - 1)) ((char*)c.resp)[c.resp_len] = '\0';
    else ((char*)c.resp)[sizeof(c.resp) - 1] = '\0';

    // Parsuj target_temp_c
    if (!hb_parse_target_temp_from_json(body, &out_settings->target_temp_c)) {
        return HB_HTTP_ERR_PARSE;
    }

    // Parsuj last_source
    if (!hb_parse_last_source_from_json(body, out_settings->last_source, 
                                       sizeof(out_settings->last_source))) {
        return HB_HTTP_ERR_PARSE;
    }

    return HB_HTTP_OK;
}

hb_http_status_t hb_http_get_settings_target_temp(
    const char *host,
    uint16_t port,
    int device_id,
    float *out_target_c,
    uint32_t timeout_ms)
{
    if (!host || !out_target_c) return HB_HTTP_ERR_PARAM;

    hb_settings_response_t settings;
    hb_http_status_t st = hb_http_get_settings(host, port, device_id, &settings, timeout_ms);
    if (st == HB_HTTP_OK) {
        *out_target_c = settings.target_temp_c;
    }
    return st;
}

// ───────────────────────── PUT /settings ─────────────────────────

hb_http_status_t hb_http_set_settings_target_temp(
    const char *host,
    uint16_t port,
    int device_id,
    float target_temp_c,
    uint32_t timeout_ms)
{
    if (!host) return HB_HTTP_ERR_PARAM;

    ip4_addr_t ip;
    if (!ip4addr_aton(host, &ip)) return HB_HTTP_ERR_IP;

    char json[128];
    int jn = hb_build_settings_json(json, sizeof(json), target_temp_c);
    if (jn < 0) return HB_HTTP_ERR_PARAM;

    char req[512];
    int rn = hb_build_http_put_settings(req, sizeof(req), host, port, device_id, json);
    if (rn < 0) return HB_HTTP_ERR_PARAM;

    hb_client_t c = {
        .pcb = NULL,
        .state = ST_IDLE,
        .err = 0,
        .req = req,
        .req_len = (uint32_t)rn,
        .req_sent = 0,
        .resp_len = 0,
    };

    // Dla PUT nie wymagamy parsowania body – liczy się 200 OK
    hb_http_status_t st = run_client(&c, &ip, port, timeout_ms);
    return st;
}

// ───────────────────────── POST /reading ─────────────────────────

hb_http_status_t hb_http_post_reading(
    const char *host,
    uint16_t port,
    int device_id,
    float temperature_c,
    float humidity_pct,
    float pressure_hpa,
    float setpoint_c,
    bool window_open_detected,
    bool is_heating,
    uint32_t timeout_ms)
{
    if (!host) return HB_HTTP_ERR_PARAM;

    ip4_addr_t ip;
    if (!ip4addr_aton(host, &ip)) return HB_HTTP_ERR_IP;

    char json[256];
    int jn = hb_build_reading_json(json, sizeof(json),
                                   temperature_c, humidity_pct, pressure_hpa, setpoint_c, window_open_detected, is_heating);
    if (jn < 0) return HB_HTTP_ERR_PARAM;

    char req[512];
    int rn = hb_build_http_post_reading(req, sizeof(req), host, port, device_id, json);
    if (rn < 0) return HB_HTTP_ERR_PARAM;

    hb_client_t c = {
        .pcb = NULL,
        .state = ST_IDLE,
        .err = 0,
        .req = req,
        .req_len = (uint32_t)rn,
        .req_sent = 0,
        .resp_len = 0,
    };

    // Dla POST nie wyragamy parsowania body – liczy się 200 OK; lwIP zamknie nam gniazdo, odbiór i tak wpadnie do bufora.
    hb_http_status_t st = run_client(&c, &ip, port, timeout_ms);
    return st;
}

#else // !ENABLE_WIFI

// Stub implementations when WiFi is disabled
hb_http_status_t hb_http_get_settings(
    const char *host, uint16_t port, int device_id,
    hb_settings_response_t *out_settings, uint32_t timeout_ms)
{
    (void)host; (void)port; (void)device_id; (void)out_settings; (void)timeout_ms;
    printf("[HTTP] WiFi disabled - skipping hb_http_get_settings\n");
    return HB_HTTP_ERR_CONNECT;
}

hb_http_status_t hb_http_get_settings_target_temp(
    const char *host, uint16_t port, int device_id,
    float *out_target_temp, uint32_t timeout_ms)
{
    (void)host; (void)port; (void)device_id; (void)out_target_temp; (void)timeout_ms;
    printf("[HTTP] WiFi disabled - skipping hb_http_get_settings_target_temp\n");
    return HB_HTTP_ERR_CONNECT;
}

hb_http_status_t hb_http_set_settings_target_temp(
    const char *host, uint16_t port, int device_id,
    float target_temp, uint32_t timeout_ms)
{
    (void)host; (void)port; (void)device_id; (void)target_temp; (void)timeout_ms;
    printf("[HTTP] WiFi disabled - skipping hb_http_set_settings_target_temp\n");
    return HB_HTTP_ERR_CONNECT;
}

hb_http_status_t hb_http_post_reading(
    const char *host, uint16_t port, int device_id,
    float temperature_c, float humidity_pct, float pressure_hpa,
    float setpoint_c, bool window_open_detected, bool is_heating, uint32_t timeout_ms)
{
    (void)host; (void)port; (void)device_id; (void)temperature_c; 
    (void)humidity_pct; (void)pressure_hpa; (void)setpoint_c; 
    (void)window_open_detected; (void)is_heating; (void)timeout_ms;
    printf("[HTTP] WiFi disabled - skipping hb_http_post_reading\n");
    return HB_HTTP_OK;
}

#endif // ENABLE_WIFI
