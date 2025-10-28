#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pico/stdio_usb.h"
#include "lvgl.h"
#include "bme280_port.h"
#include "../lv_port/lv_port_disp.h"
#include "../lv_port/lv_port_indev.h"
#include "bsp_i2c.h"
#include "bsp_pcf85063.h"
#include "lvgl_ui/screen/main_screen.h"

// === WIFI / LWIP (RM2: CYW43439)
#include "pico/cyw43_arch.h"

// lwIP – wariant bez BSD-socketów: używamy API netconn
#include "lwip/netif.h"
#include "lwip/ip4_addr.h"
#include "lwip/ip_addr.h"
#include "lwip/api.h"       // netconn_*
#include "lwip/inet.h"      // ipaddr_aton
#include "lwip/netbuf.h"    // netbuf_*

// Sprawdź, czy lwIP netconn jest dostępne (SDK 2.1.0 ARM nie wspiera SYS!)
#if !defined(LWIP_NETCONN) || LWIP_NETCONN==0
#  warning "LWIP_NETCONN==0 - SDK 2.1.0 ARM nie wspiera lwIP SYS. HTTP client wyłączony."
#  undef ENABLE_HTTP_CLIENT
#  define ENABLE_HTTP_CLIENT 0
#endif

// Włącz/wyłącz prostego klienta HTTP
#ifndef ENABLE_HTTP_CLIENT
#define ENABLE_HTTP_CLIENT 1
#endif

#define LVGL_TICK_MS 5
#define DISP_HOR_RES 466
#define DISP_VER_RES 466

#ifndef WIFI_SSID
#define WIFI_SSID "KAMNET_8960"
#endif
#ifndef WIFI_PASS
#define WIFI_PASS "Pawianywchodzanasciany"
#endif

// Backend – używamy IP literalnego (bez DNS)
#ifndef HEATBEAT_API_HOST
#define HEATBEAT_API_HOST "192.168.55.120"
#endif
#ifndef HEATBEAT_API_PORT
#define HEATBEAT_API_PORT 8000
#endif

// Timery synchronizacji
#define HTTP_GET_INTERVAL_MS  30000  // GET co 30s
#define HTTP_POST_INTERVAL_MS 10000  // POST current_temp co 10s

// ===== Struktura danych aplikacji =====
typedef struct {
    float target_temp;      // Temperatura docelowa z backendu
    float current_temp;     // Aktualna temperatura (BME280)
    int humidity;           // Wilgotność
    float pressure;         // Ciśnienie (Pa)
    bool backend_available; // Czy backend odpowiada
    uint32_t last_get_ms;   // Timestamp ostatniego GET
    uint32_t last_post_ms;  // Timestamp ostatniego POST
} heatbeat_state_t;

static heatbeat_state_t g_state = {
    .target_temp = 22.0f,
    .current_temp = 0.0f,
    .humidity = 0,
    .pressure = 0.0f,
    .backend_available = false,
    .last_get_ms = 0,
    .last_post_ms = 0
};

// LVGL tick timer
static bool tick_cb(struct repeating_timer *t) { lv_tick_inc(LVGL_TICK_MS); return true; }

// Print free RAM (RP2350-specific)
extern char __StackLimit, __bss_end__;
static void print_free_ram(const char* msg) {
    uint32_t free_ram = (uint32_t)&__StackLimit - (uint32_t)&__bss_end__;
    printf("[RAM] %s: Wolna RAM: %lu bajtów\n", msg, (unsigned long)free_ram);
}

static void print_ip4(const ip4_addr_t* ip) {
    printf("%u.%u.%u.%u", ip4_addr1(ip), ip4_addr2(ip), ip4_addr3(ip), ip4_addr4(ip));
}

/* Pomocniczo: aktywny interfejs (zwykle jedyny) */
static inline struct netif* get_nif(void) {
    return netif_default ? netif_default : netif_list;
}

static int try_wifi_auth(uint32_t auth)
{
    printf("[WiFi] próba połączenia (auth=0x%08lx) do \"%s\"...\n", (unsigned long)auth, WIFI_SSID);
    int rc = cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASS, auth, 20000);
    if (rc) {
        printf("[WiFi] NIE połączono (rc=%d)\n", rc);
        return rc;
    }

    struct netif* nif = get_nif();
    if (!nif || !netif_is_up(nif)) {
        printf("[WiFi] interfejs nie jest UP\n");
        return -1;
    }
    printf("[WiFi] Połączono z \"%s\"  IP:", WIFI_SSID);
    print_ip4(netif_ip4_addr(nif));
    printf("  GW: "); print_ip4(netif_ip4_gw(nif));
    printf("  MASK: "); print_ip4(netif_ip4_netmask(nif));
    printf("\n");

    int rssi = cyw43_wifi_get_rssi(&cyw43_state, CYW43_ITF_STA);
    if (rssi != 0) printf("[WiFi] RSSI: %d dBm\n", rssi);
    return 0;
}

static bool wifi_connect_and_log(void) {
    printf("[BOOT] Inicjalizacja CYW43...\n");
    if (cyw43_arch_init_with_country(CYW43_COUNTRY_POLAND)) {
        printf("❌ cyw43_arch_init_with_country() failed\n");
        return false;
    }
    cyw43_arch_enable_sta_mode();

    const uint32_t try_auths[] = {
        CYW43_AUTH_WPA2_AES_PSK,
        CYW43_AUTH_WPA2_MIXED_PSK,
        CYW43_AUTH_WPA_TKIP_PSK,
        CYW43_AUTH_OPEN
    };
    for (size_t i = 0; i < sizeof(try_auths)/sizeof(try_auths[0]); ++i) {
        if (try_wifi_auth(try_auths[i]) == 0) return true;
        sleep_ms(500);
    }
    printf("❌ Nie udało się połączyć z \"%s\" – sprawdź hasło/SSID.\n", WIFI_SSID);
    return false;
}

/* Periodyczny status Wi-Fi do terminala */
static void wifi_status_print_once(void) {
    int st = cyw43_wifi_link_status(&cyw43_state, CYW43_ITF_STA);
    const char* s = "UNK";
    switch (st) {
        case CYW43_LINK_DOWN:   s = "DOWN"; break;
        case CYW43_LINK_JOIN:   s = "JOIN"; break;
        case CYW43_LINK_NOIP:   s = "NOIP"; break;
        case CYW43_LINK_UP:     s = "UP";   break;
        case CYW43_LINK_FAIL:   s = "FAIL"; break;
        case CYW43_LINK_NONET:  s = "NONET";break;
        case CYW43_LINK_BADAUTH:s = "BADAUTH"; break;
        default: break;
    }
    struct netif* nif = get_nif();
    printf("[WiFi] %s  IP=", s);
    if (nif && netif_is_up(nif)) {
        if (!ip4_addr_isany_val(*netif_ip4_addr(nif))) {
            print_ip4(netif_ip4_addr(nif));
        } else {
            printf("0.0.0.0");
        }
    } else {
        printf("0.0.0.0");
    }
    printf("\n");
}

#if ENABLE_HTTP_CLIENT
/* ===== HTTP client (netconn API) ===== */

static struct netconn* netconn_connect_host(const char* host_ip, uint16_t port)
{
    ip_addr_t ip;
    if (!ipaddr_aton(host_ip, &ip)) {
        printf("[HTTP] ipaddr_aton() fail dla '%s'\n", host_ip);
        return NULL;
    }

    struct netconn* nc = netconn_new(NETCONN_TCP);
    if (!nc) {
        printf("[HTTP] netconn_new() fail\n");
        return NULL;
    }

    // Ustaw timeout na recv (5 sekund)
    netconn_set_recvtimeout(nc, 5000);

    err_t err = netconn_connect(nc, &ip, port);
    if (err != ERR_OK) {
        printf("[HTTP] netconn_connect() err=%d\n", (int)err);
        netconn_delete(nc);
        return NULL;
    }
    return nc;
}

static int http_exchange(
    struct netconn* nc,
    const char* method,           // "GET" lub "POST"
    const char* path,             // np. "/device/1/settings"
    const char* host_header,      // np. "192.168.55.120:8000"
    const char* body,             // body dla POST (może być NULL)
    char* out_buf, size_t out_sz  // wyjściowy bufor na body odpowiedzi
)
{
    char req[768];
    int n;
    
    if (body && body[0]) {
        // POST z body
        n = snprintf(req, sizeof(req),
            "%s %s HTTP/1.1\r\n"
            "Host: %s\r\n"
            "Content-Type: application/json\r\n"
            "Content-Length: %d\r\n"
            "Connection: close\r\n"
            "\r\n"
            "%s",
            method, path, host_header, (int)strlen(body), body
        );
    } else {
        // GET lub POST bez body
        n = snprintf(req, sizeof(req),
            "%s %s HTTP/1.1\r\n"
            "Host: %s\r\n"
            "Connection: close\r\n"
            "\r\n",
            method, path, host_header
        );
    }
    
    if (n <= 0 || (size_t)n >= sizeof(req)) {
        printf("[HTTP] req overflow\n");
        return -1;
    }

    err_t err = netconn_write(nc, req, (size_t)n, NETCONN_COPY);
    if (err != ERR_OK) {
        printf("[HTTP] netconn_write() err=%d\n", (int)err);
        return -1;
    }

    // Odbiór odpowiedzi
    size_t used = 0;
    struct netbuf* inbuf;
    while ((err = netconn_recv(nc, &inbuf)) == ERR_OK) {
        void* data;
        u16_t len;
        do {
            netbuf_data(inbuf, &data, &len);
            size_t take = (used + len <= out_sz) ? len : (out_sz - used);
            if (take) {
                memcpy(out_buf + used, data, take);
                used += take;
            }
        } while (netbuf_next(inbuf) >= 0);
        netbuf_delete(inbuf);
    }

    if (used == 0) return -1;

    // Znajdź początek BODY po podwójnym CRLF
    const char* hdr_end = NULL;
    for (size_t i = 3; i < used; ++i) {
        if (out_buf[i-3]=='\r' && out_buf[i-2]=='\n' && out_buf[i-1]=='\r' && out_buf[i]=='\n') {
            hdr_end = out_buf + i + 1;
            break;
        }
    }
    if (!hdr_end) return -1;

    size_t body_bytes = (out_buf + used) - hdr_end;
    memmove(out_buf, hdr_end, body_bytes);
    if (body_bytes < out_sz) out_buf[body_bytes] = '\0';
    return (int)body_bytes;
}

// ===== HTTP GET: pobierz target_temp z backendu =====
static bool http_get_target_temp(float* out_temp)
{
    struct netconn* nc = netconn_connect_host(HEATBEAT_API_HOST, HEATBEAT_API_PORT);
    if (!nc) {
        printf("[HTTP GET] Nie można połączyć z backendem\n");
        return false;
    }

    char response[512];
    char host_hdr[64];
    snprintf(host_hdr, sizeof(host_hdr), "%s:%d", HEATBEAT_API_HOST, HEATBEAT_API_PORT);

    int got = http_exchange(nc, "GET", "/device/1/settings", host_hdr, NULL, response, sizeof(response) - 1);
    netconn_close(nc);
    netconn_delete(nc);

    if (got <= 0) {
        printf("[HTTP GET] Błąd odczytu odpowiedzi\n");
        return false;
    }

    response[got] = '\0';
    printf("[HTTP GET] /device/1/settings -> %s\n", response);

    // Parse JSON: {"target_temp": 22.5}
    char* ptr = strstr(response, "\"target_temp\"");
    if (ptr) {
        ptr = strchr(ptr, ':');
        if (ptr) {
            float temp = 0.0f;
            if (sscanf(ptr + 1, "%f", &temp) == 1) {
                *out_temp = temp;
                printf("[HTTP GET] Odczytano target_temp=%.1f°C\n", temp);
                return true;
            }
        }
    }

    printf("[HTTP GET] Nie znaleziono target_temp w odpowiedzi\n");
    return false;
}

// ===== HTTP POST: wyślij aktualną temperaturę do backendu =====
static bool http_post_current_temp(float temp, int humidity, float pressure)
{
    struct netconn* nc = netconn_connect_host(HEATBEAT_API_HOST, HEATBEAT_API_PORT);
    if (!nc) {
        printf("[HTTP POST] Nie można połączyć z backendem\n");
        return false;
    }

    char body[256];
    snprintf(body, sizeof(body),
        "{\"temperature\":%.2f,\"humidity\":%d,\"pressure\":%.2f}",
        temp, humidity, pressure
    );

    char response[512];
    char host_hdr[64];
    snprintf(host_hdr, sizeof(host_hdr), "%s:%d", HEATBEAT_API_HOST, HEATBEAT_API_PORT);

    int got = http_exchange(nc, "POST", "/device/1/temperature", host_hdr, body, response, sizeof(response) - 1);
    netconn_close(nc);
    netconn_delete(nc);

    if (got <= 0) {
        printf("[HTTP POST] Błąd wysyłania temperatury\n");
        return false;
    }

    response[got] = '\0';
    printf("[HTTP POST] /device/1/temperature -> %s\n", response);
    return true;
}

// ===== HTTP POST: zmień target_temp na backendzie =====
static bool http_post_target_temp(float new_target)
{
    struct netconn* nc = netconn_connect_host(HEATBEAT_API_HOST, HEATBEAT_API_PORT);
    if (!nc) {
        printf("[HTTP POST] Nie można połączyć z backendem\n");
        return false;
    }

    char body[128];
    snprintf(body, sizeof(body), "{\"target_temp\":%.1f}", new_target);

    char response[512];
    char host_hdr[64];
    snprintf(host_hdr, sizeof(host_hdr), "%s:%d", HEATBEAT_API_HOST, HEATBEAT_API_PORT);

    int got = http_exchange(nc, "POST", "/device/1/target_temp", host_hdr, body, response, sizeof(response) - 1);
    netconn_close(nc);
    netconn_delete(nc);

    if (got <= 0) {
        printf("[HTTP POST] Błąd zmiany target_temp\n");
        return false;
    }

    response[got] = '\0';
    printf("[HTTP POST] /device/1/target_temp -> %s\n", response);
    return true;
}
#endif // ENABLE_HTTP_CLIENT

// ===== Callback z UI: użytkownik zmienił target_temp =====
void heatbeat_on_target_temp_changed(float new_target)
{
#if ENABLE_HTTP_CLIENT
    printf("[UI] Użytkownik zmienił target_temp na %.1f°C\n", new_target);
    g_state.target_temp = new_target;
    
    // Wyślij POST do backendu
    if (http_post_target_temp(new_target)) {
        printf("[UI] Pomyślnie zaktualizowano target_temp na backendzie\n");
    } else {
        printf("[UI] Błąd aktualizacji target_temp na backendzie\n");
    }
#endif
}

int main(void) {
    stdio_usb_init();

    // Dodatkowe 10 s na podłączenie PuTTY
    absolute_time_t t_limit = make_timeout_time_ms(10000);
    while (!stdio_usb_connected() && absolute_time_diff_us(get_absolute_time(), t_limit) > 0) {
        sleep_ms(50);
    }

    printf("\r\n=== HeatBeat-Pico start ===\r\n");
    print_free_ram("Boot");

    // === WIFI: połącz i zaloguj IP
    printf("[BOOT] Start Wi-Fi init...\n");
    bool wifi_ok = wifi_connect_and_log();

    // Inicjalizacja UI
    printf("[BOOT] Init I2C/RTC/LVGL...\n");
    bsp_i2c_init();
    bsp_pcf85063_init();

    lv_init();
    lv_port_disp_init(DISP_HOR_RES, DISP_VER_RES, 0, false);
    lv_port_indev_init(DISP_HOR_RES, DISP_VER_RES, 0);

    bme280_init_default();

    main_screen_init();
    lv_scr_load(ui_main_screen);

    static struct repeating_timer t;
    add_repeating_timer_ms(LVGL_TICK_MS, tick_cb, NULL, &t);

    printf("[BOOT] Main loop.\n");

    // Timery
    uint32_t last_read = to_ms_since_boot(get_absolute_time());
    uint32_t last_time = last_read;
    uint32_t last_bme_print = last_read;
    uint32_t last_wifi_status_print = last_read;

    g_state.last_get_ms = last_read;
    g_state.last_post_ms = last_read;

    // Do wykrywania zmian
    int last_status = -999;
    uint32_t last_ip_raw = 0;

    struct tm now_tm;
    struct bme280_data bme_data;

    while (true) {
    cyw43_arch_poll();  // ✅ KLUCZOWE dla NO_SYS (poll) - obsługa WiFi
    
    lv_timer_handler();
    sleep_ms(LVGL_TICK_MS);

        uint32_t now = to_ms_since_boot(get_absolute_time());

        // Zegar na ekranie
        if (now - last_time > 1000) {
            bsp_pcf85063_get_time(&now_tm);
            char buf[32];
            snprintf(buf, sizeof(buf), "%02d:%02d:%02d", now_tm.tm_hour, now_tm.tm_min, now_tm.tm_sec);
            if (label_time) lv_label_set_text(label_time, buf);
            last_time = now;
        }

        // BME280: odczyt co ~2 s, log do terminala co 10 s
        if (now - last_read > 2000) {
            if (bme280_read_data(&bme_data) == 0) {
                g_state.current_temp = bme_data.temperature;
                g_state.humidity = (int)(bme_data.humidity + 0.5f);
                g_state.pressure = bme_data.pressure;
                
                extern void update_labels(void);
                update_labels();

                if (now - last_bme_print > 10000) {
                    printf("[BME] T=%.2f°C RH=%d%% P=%.2f hPa\n",
                           g_state.current_temp, g_state.humidity, g_state.pressure / 100.0f);
                    last_bme_print = now;
                }
            }
            last_read = now;
        }

        // ——— Wi-Fi: periodyczny status
        if (now - last_wifi_status_print > 10000) { // co 10 s
            int st = cyw43_wifi_link_status(&cyw43_state, CYW43_ITF_STA);
            struct netif* nif = get_nif();
            uint32_t ip_raw = (nif && netif_is_up(nif)) ? netif_ip4_addr(nif)->addr : 0;

            if (st != last_status || ip_raw != last_ip_raw) {
                wifi_status_print_once();
                last_status = st;
                last_ip_raw = ip_raw;
            } else {
                wifi_status_print_once();
            }
            last_wifi_status_print = now;
        }

#if ENABLE_HTTP_CLIENT
        if (wifi_ok) {
            // HTTP GET: pobierz target_temp co 30s
            if (now - g_state.last_get_ms > HTTP_GET_INTERVAL_MS) {
                float target = 0.0f;
                if (http_get_target_temp(&target)) {
                    g_state.target_temp = target;
                    g_state.backend_available = true;
                    
                    // Zaktualizuj UI (jeśli masz label dla target_temp)
                    // extern void update_target_temp_label(float temp);
                    // update_target_temp_label(target);
                } else {
                    g_state.backend_available = false;
                }
                g_state.last_get_ms = now;
            }

            // HTTP POST: wyślij current_temp co 10s
            if (now - g_state.last_post_ms > HTTP_POST_INTERVAL_MS) {
                if (http_post_current_temp(g_state.current_temp, g_state.humidity, g_state.pressure)) {
                    g_state.backend_available = true;
                } else {
                    g_state.backend_available = false;
                }
                g_state.last_post_ms = now;
            }
        }
#endif
    }
}