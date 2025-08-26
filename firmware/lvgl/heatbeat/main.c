#include <stdio.h>
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

/* tylko do wypisania IP – NIE powoduje konfliktów */
#include "lwip/netif.h"
#include "lwip/ip4_addr.h"

#ifndef ENABLE_HTTP_CLIENT
#define ENABLE_HTTP_CLIENT 0   // domyślnie wyłączone; zmień na 1 jeśli chcesz domyślnie włączyć
#endif

#if ENABLE_HTTP_CLIENT
  #include "lwip/sockets.h"
  #include "lwip/inet.h"
  #include "lwip/dns.h"
#endif


#define LVGL_TICK_MS 5
#define DISP_HOR_RES 466
#define DISP_VER_RES 466

// Możesz wyłączyć HTTP klienta, ustawiając 0
#ifndef ENABLE_HTTP_CLIENT
#define ENABLE_HTTP_CLIENT 0
#endif


#ifndef WIFI_SSID
#define WIFI_SSID "YOUR_SSID"
#endif
#ifndef WIFI_PASS
#define WIFI_PASS "YOUR_PASS"
#endif

// Adres bazy/API (plain HTTP) – np. FastAPI na twoim serwerze/LAN
#ifndef HEATBEAT_API_BASE
#define HEATBEAT_API_BASE "http://192.168.0.100:8000"
#endif

// LVGL tick timer
static bool tick_cb(struct repeating_timer *t) { lv_tick_inc(LVGL_TICK_MS); return true; }

// Print free RAM (RP2040-specific)
extern char __StackLimit, __bss_end__;
static void print_free_ram(const char* msg) {
    uint32_t free_ram = (uint32_t)&__StackLimit - (uint32_t)&__bss_end__;
    printf("[RAM] %s: Wolna RAM: %lu bajtów\n", msg, free_ram);
}

// === WIFI: pomocnicze
static void print_ip4(const ip4_addr_t* ip) {
    printf("%u.%u.%u.%u", ip4_addr1(ip), ip4_addr2(ip), ip4_addr3(ip), ip4_addr4(ip));
}

static bool wifi_connect_and_log(void) {
    printf("➡️ Inicjalizacja układu CYW43 (RM2)...\n");
    if (cyw43_arch_init_with_country(CYW43_COUNTRY_POLAND)) {
        printf("❌ cyw43_arch_init_with_country() failed\n");
        return false;
    }
    cyw43_arch_enable_sta_mode();
    printf("ℹ️ Łączenie z Wi‑Fi SSID=\"%s\" ...\n", WIFI_SSID);

    int rc = cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASS, CYW43_AUTH_WPA2_AES_PSK, 25000);
    if (rc) {
        printf("❌ Nie udało się połączyć z Wi‑Fi (kod=%d)\n", rc);
        return false;
    }

    struct netif* nif = netif_list; // pierwsza (i zwykle jedyna) netif
    if (!nif || !netif_is_up(nif)) {
        printf("❌ Interfejs sieciowy nie jest UP\n");
        return false;
    }

    printf("✅ Połączono z \"%s\"\n", WIFI_SSID);
    printf("IP: ");  print_ip4(netif_ip4_addr(nif));
    printf("  GW: "); print_ip4(netif_ip4_gw(nif));
    printf("  MASK: "); print_ip4(netif_ip4_netmask(nif));
    printf("\n");

    int rssi = cyw43_wifi_get_rssi(&cyw43_state, CYW43_ITF_STA);
    if (rssi != 0) {
        printf("RSSI: %d dBm\n", rssi);
    }
    return true;
}

// === HTTP mini-klient (plain HTTP/1.1) – tylko jeśli włączono
#if ENABLE_HTTP_CLIENT
static int http_request(const char* method, const char* url_path, const char* host,
                        const char* body, char* resp_buf, size_t resp_buf_sz) {
    // DNS
    ip_addr_t addr;
    if (netconn_gethostbyname(host, &addr) != ERR_OK) {
        printf("DNS fail for host %s\n", host);
        return -1;
    }

    int sock = lwip_socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (sock < 0) return -2;

    struct sockaddr_in sa = {0};
    sa.sin_family = AF_INET;
    sa.sin_port = htons(80);
    sa.sin_addr.s_addr = addr.u_addr.ip4.addr;

    if (lwip_connect(sock, (struct sockaddr*)&sa, sizeof(sa)) < 0) {
        lwip_close(sock);
        return -3;
    }

    char req[1024];
    int blen = body ? (int)strlen(body) : 0;

    if (body) {
        int hdr_len = snprintf(req, sizeof(req),
            "%s %s HTTP/1.1\r\n"
            "Host: %s\r\n"
            "User-Agent: HeatBeat-Pico/1.0\r\n"
            "Accept: application/json\r\n"
            "Content-Type: application/json\r\n"
            "Content-Length: %d\r\n"
            "Connection: close\r\n"
            "\r\n"
            "%s",
            method, url_path, host, blen, body);
        if (lwip_write(sock, req, hdr_len) < 0) { lwip_close(sock); return -4; }
    } else {
        int req_len = snprintf(req, sizeof(req),
            "%s %s HTTP/1.1\r\n"
            "Host: %s\r\n"
            "User-Agent: HeatBeat-Pico/1.0\r\n"
            "Accept: application/json\r\n"
            "Connection: close\r\n"
            "\r\n",
            method, url_path, host);
        if (lwip_write(sock, req, req_len) < 0) { lwip_close(sock); return -4; }
    }

    int total = 0;
    while (total < (int)resp_buf_sz - 1) {
        int n = lwip_read(sock, resp_buf + total, (int)resp_buf_sz - 1 - total);
        if (n <= 0) break;
        total += n;
    }
    resp_buf[total] = 0;
    lwip_close(sock);
    return total;
}

static int parse_url_base(const char* full, char* host, size_t host_sz, char* base_path, size_t path_sz) {
    // "http://host[:port]/xyz"
    const char* p = strstr(full, "://");
    if (!p) return -1;
    p += 3;
    const char* slash = strchr(p, '/');
    if (!slash) slash = p + strlen(p);
    size_t hlen = (size_t)(slash - p);
    if (hlen >= host_sz) return -2;
    memcpy(host, p, hlen); host[hlen] = 0;

    if (*slash) snprintf(base_path, path_sz, "%s", slash);
    else snprintf(base_path, path_sz, "/");
    return 0;
}

static int api_post_telemetry(float t, float rh, float p_hpa) {
    char host[128], base_path[256];
    if (parse_url_base(HEATBEAT_API_BASE, host, sizeof(host), base_path, sizeof(base_path)) != 0) return -1;

    char path[256];
    snprintf(path, sizeof(path), "%s/api/telemetry", base_path);

    char body[256];
    snprintf(body, sizeof(body), "{\"temp\":%.2f,\"hum\":%.1f,\"pres\":%.2f}", t, rh, p_hpa);

    char resp[1024];
    int n = http_request("POST", path, host, body, resp, sizeof(resp));
    if (n < 0) {
        printf("POST /telemetry ERROR %d\n", n);
        return n;
    }
    printf("POST /telemetry OK (%dB)\n", n);
    return 0;
}

static int api_get_target(float* out_target) {
    char host[128], base_path[256];
    if (parse_url_base(HEATBEAT_API_BASE, host, sizeof(host), base_path, sizeof(base_path)) != 0) return -1;

    char path[256];
    snprintf(path, sizeof(path), "%s/api/target-temp", base_path);

    char resp[1024];
    int n = http_request("GET", path, host, NULL, resp, sizeof(resp));
    if (n < 0) {
        printf("GET /target-temp ERROR %d\n", n);
        return n;
    }
    // Naiwne wyłuskanie "target":<float>
    char* p = strstr(resp, "\"target\"");
    if (!p) return -10;
    p = strchr(p, ':');
    if (!p) return -11;
    *out_target = strtof(p + 1, NULL);
    printf("🎯 Target=%.2f°C\n", *out_target);
    return 0;
}
#endif // ENABLE_HTTP_CLIENT

int main(void) {
    stdio_usb_init();
    printf("\r\n--- HeatBeat-Pico start! ---\r\n");
    print_free_ram("Boot");

    // (Opcjonalnie) czekać chwilę na terminal
    // sleep_ms(2000);

    // === WIFI: połącz i zaloguj IP (jeśli RM2 podłączony do 23/24/25/29)
    bool wifi_ok = wifi_connect_and_log();

    // Inicjalizacja hardware
    bsp_i2c_init();
    bsp_pcf85063_init();

    lv_init();
    lv_port_disp_init(DISP_HOR_RES, DISP_VER_RES, 0, false);
    lv_port_indev_init(DISP_HOR_RES, DISP_VER_RES, 0);

    // Inicjalizacja czujnika BME280
    bme280_init_default();

    // Inicjalizacja głównego ekranu UI
    main_screen_init();
    lv_scr_load(ui_main_screen);

    // Timer LVGL
    static struct repeating_timer t;
    add_repeating_timer_ms(LVGL_TICK_MS, tick_cb, NULL, &t);

    // Timery do update'ów
    uint32_t last_read = to_ms_since_boot(get_absolute_time());
    uint32_t last_time = last_read;
    uint32_t last_telemetry = last_read;
    uint32_t last_target_poll = last_read;

    struct tm now_tm;
    struct bme280_data bme_data;

    // Cache target-temp z serwera
    float target_temp_cache = 0.0f;

    while (true) {
        lv_timer_handler();
        sleep_ms(LVGL_TICK_MS);

        uint32_t now = to_ms_since_boot(get_absolute_time());

        // Aktualizacja czasu na ekranie
        if (now - last_time > 1000) {
            bsp_pcf85063_get_time(&now_tm);
            char buf[32];
            snprintf(buf, sizeof(buf), "%02d:%02d:%02d", now_tm.tm_hour, now_tm.tm_min, now_tm.tm_sec);
            if (label_time) lv_label_set_text(label_time, buf);
            last_time = now;
        }

        // Aktualizacja wartości z BME280 co 2 sekundy
        if (now - last_read > 2000) {
            if (bme280_read_data(&bme_data) == 0) {
                extern float current_temp;
                extern int humidity;
                extern float pressure;
                current_temp = bme_data.temperature;
                humidity = (int)(bme_data.humidity + 0.5f);
                pressure = bme_data.pressure;
                extern void update_labels(void);
                update_labels();

                printf("BME: T=%.2f°C RH=%d%% P=%.2f hPa\n",
                       current_temp, humidity, pressure / 100.0f);
            }
            last_read = now;
        }

#if ENABLE_HTTP_CLIENT
        // Wysyłaj telemetrię co ~10 s, jeśli Wi‑Fi OK
        if (wifi_ok && (now - last_telemetry > 10000)) {
            float t = bme_data.temperature;
            float rh = bme_data.humidity;
            float p_hpa = bme_data.pressure / 100.0f;
            api_post_telemetry(t, rh, p_hpa);
            last_telemetry = now;
        }

        // Co ~30 s sprawdź temperaturę zadaną z serwera
        if (wifi_ok && (now - last_target_poll > 30000)) {
            float tgt = 0.0f;
            if (api_get_target(&tgt) == 0) {
                target_temp_cache = tgt;
                // Tu możesz dodać aktualizację UI (np. label_target) / sterowanie
            }
            last_target_poll = now;
        }
#endif
    }
}
