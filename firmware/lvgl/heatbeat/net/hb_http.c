// hb_http.c
// Minimalny, jednorazowy klient TCP oparty na lwIP raw API.
// Obsługuje GET /device/{id}/settings oraz POST /device/{id}/reading.

#include "hb_http.h"
#include "hb_proto.h"
#include "pico/cyw43_arch.h"

#include "lwip/ip4_addr.h"
#include "lwip/tcp.h"
#include "pico/time.h"
#include <string.h>
#include <stdio.h>

#ifndef HB_HTTP_RECV_BUF_MAX
#define HB_HTTP_RECV_BUF_MAX 1024
#endif

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
    if (lwip_err != ERR_OK) {
        c->err = HB_HTTP_ERR_CONNECT;
        c->state = ST_ERROR;
        return lwip_err;
    }
    tcp_arg(tpcb, c);
    tcp_err(tpcb, on_err);
    tcp_recv(tpcb, NULL);
    tcp_sent(tpcb, on_sent);    // KLUCZOWE

    c->state = ST_SENDING;
    c->pcb = tpcb;

    // natychmiast wypchnij pierwszą porcję
    u16_t space = tcp_sndbuf(tpcb);
    if (space > 0 && c->req_sent < c->req_len) {
        u16_t chunk = (u16_t)(c->req_len - c->req_sent);
        if (chunk > space) chunk = space;
        err_t w = tcp_write(tpcb, c->req + c->req_sent, chunk, TCP_WRITE_FLAG_COPY);
        if (w == ERR_OK) tcp_output(tpcb);
    }
    return ERR_OK;
}

static err_t on_sent(void *arg, struct tcp_pcb *tpcb, u16_t len) {
    hb_client_t *c = (hb_client_t *)arg;
    c->req_sent += len;

    if (c->req_sent >= c->req_len) {
        c->state = ST_RECEIVING;
        tcp_recv(tpcb, on_recv);
    } else {
        u16_t space = tcp_sndbuf(tpcb);
        if (space > 0) {
            u16_t chunk = (u16_t)(c->req_len - c->req_sent);
            if (chunk > space) chunk = space;
            err_t w = tcp_write(tpcb, c->req + c->req_sent, chunk, TCP_WRITE_FLAG_COPY);
            if (w == ERR_OK) tcp_output(tpcb);
            else if (w != ERR_MEM) {
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

    if (!p) {
        c->state = ST_DONE;
        return ERR_OK;
    }
    if (err != ERR_OK) {
        pbuf_free(p);
        c->err = HB_HTTP_ERR_RECV;
        c->state = ST_ERROR;
        return err;
    }

    struct pbuf *q = p;
    while (q && c->resp_len < sizeof(c->resp)) {
        u16_t copy = q->len;
        if (copy > (u16_t)(sizeof(c->resp) - c->resp_len))
            copy = (u16_t)(sizeof(c->resp) - c->resp_len);
        memcpy(c->resp + c->resp_len, q->payload, copy);
        c->resp_len += copy;
        q = q->next;
    }
    tcp_recved(tpcb, p->tot_len);
    pbuf_free(p);
    return ERR_OK;
}

static void on_err(void *arg, err_t lwip_err) {
    hb_client_t *c = (hb_client_t *)arg;
    (void)lwip_err;
    if (c->state != ST_DONE) {
        c->err = HB_HTTP_ERR_CONNECT;
        c->state = ST_ERROR;
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
    c->pcb = tcp_new_ip_type(IPADDR_TYPE_V4);
    if (!c->pcb) return HB_HTTP_ERR_CONNECT;

    tcp_arg(c->pcb, c);
    tcp_err(c->pcb, on_err);

    c->state = ST_CONNECTING;
    err_t e = tcp_connect(c->pcb, ip, port, on_connected);
    if (e != ERR_OK) {
        tcp_close(c->pcb);
        c->pcb = NULL;
        return HB_HTTP_ERR_CONNECT;
    }

    absolute_time_t deadline = make_timeout_time_ms(timeout_ms);
    while (c->state != ST_DONE && c->state != ST_ERROR) {
        cyw43_arch_poll();
        tight_loop_contents();
        if (absolute_time_diff_us(get_absolute_time(), deadline) <= 0) {
            c->err = HB_HTTP_ERR_TIMEOUT;
            c->state = ST_ERROR;
            break;
        }

        if (c->state == ST_SENDING && c->req_sent < c->req_len) {
            u16_t space = tcp_sndbuf(c->pcb);
            if (space > 0) {
                u16_t chunk = (u16_t)(c->req_len - c->req_sent);
                if (chunk > space) chunk = space;
                err_t w = tcp_write(c->pcb, c->req + c->req_sent, chunk, TCP_WRITE_FLAG_COPY);
                if (w == ERR_OK) tcp_output(c->pcb);
                else if (w != ERR_MEM) {
                    c->err = HB_HTTP_ERR_SEND;
                    c->state = ST_ERROR;
                }
            }
        }
    }

    if (c->pcb) {
        tcp_arg(c->pcb, NULL);
        tcp_recv(c->pcb, NULL);
        tcp_sent(c->pcb, NULL);
        tcp_err(c->pcb, NULL);
        tcp_close(c->pcb);
        c->pcb = NULL;
    }
    return (c->state == ST_DONE) ? HB_HTTP_OK : (hb_http_status_t)c->err;
}

// ───────────────────────── GET /settings ─────────────────────────

hb_http_status_t hb_http_get_settings_target_temp(
    const char *host,
    uint16_t port,
    int device_id,
    float *out_target_c,
    uint32_t timeout_ms)
{
    if (!host || !out_target_c) return HB_HTTP_ERR_PARAM;

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

    if (!hb_parse_target_temp_from_json(body, out_target_c)) return HB_HTTP_ERR_PARSE;
    return HB_HTTP_OK;
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
    uint32_t timeout_ms)
{
    if (!host) return HB_HTTP_ERR_PARAM;

    ip4_addr_t ip;
    if (!ip4addr_aton(host, &ip)) return HB_HTTP_ERR_IP;

    char json[256];
    int jn = hb_build_reading_json(json, sizeof(json),
                                   temperature_c, humidity_pct, pressure_hpa, setpoint_c);
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

    // Dla POST nie wymagamy parsowania body – liczy się 200 OK; lwIP zamknie nam gniazdo, odbiór i tak wpadnie do bufora.
    hb_http_status_t st = run_client(&c, &ip, port, timeout_ms);
    return st;
}
