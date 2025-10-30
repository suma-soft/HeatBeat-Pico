#include <stdio.h>
#include <stdint.h>
#include <time.h>
#include <math.h>
#include "pico/stdlib.h"
#include "pico/stdio_usb.h"
#include "pico/time.h"
#include "hardware/gpio.h"

#include "lvgl.h"
#include "bme280_port.h"
#include "../lv_port/lv_port_disp.h"
#include "../lv_port/lv_port_indev.h"
#include "bsp_i2c.h"
#include "bsp_pcf85063.h"
#include "lvgl_ui/screen/main_screen.h"

#ifndef ENABLE_WIFI
#define ENABLE_WIFI 1
#endif
#ifndef ENABLE_HTTP_CLIENT
#define ENABLE_HTTP_CLIENT 1
#endif

#if ENABLE_WIFI
  #include "pico/cyw43_arch.h"
  #include "lwip/netif.h"
  #include "lwip/ip4_addr.h"
  #if ENABLE_HTTP_CLIENT
    #include "lwip/sockets.h"
    #include "lwip/inet.h"
    #include "lwip/dns.h"
  #endif
#endif

#include "net/hb_http.h"
#include "net/hb_proto.h"

#define LVGL_TICK_MS 5
#define DISP_HOR_RES 466
#define DISP_VER_RES 466

#ifndef WIFI_SSID
#define WIFI_SSID "KAMNET_8960"
#endif
#ifndef WIFI_PASS
#define WIFI_PASS "Pawianywchodzanasciany"
#endif

#ifndef HB_HOST
#define HB_HOST "192.168.55.120"
#endif
#ifndef HB_PORT
#define HB_PORT 8000
#endif
#ifndef HB_DEVICE_ID
#define HB_DEVICE_ID 1
#endif

/* Jeżeli masz endpoint ustawień (POST/PUT /device/{id}/settings),
 * włącz i zaimplementuj hb_http_set_settings_target_temp():
 */
// #define HB_HAVE_SET_ENDPOINT 1

#ifndef BOOT_DIAG_LED
#define BOOT_DIAG_LED 25
#endif

static bool tick_cb(struct repeating_timer *t) { (void)t; lv_tick_inc(LVGL_TICK_MS); return true; }

extern char __StackLimit, __bss_end__;
static void print_free_ram(const char* msg) {
    uint32_t free_ram = (uint32_t)&__StackLimit - (uint32_t)&__bss_end__;
    printf("[RAM] %s: Wolna RAM: %lu bajtów\n", msg, (unsigned long)free_ram);
}

#if ENABLE_WIFI
static void print_ip4(const ip4_addr_t* ip) {
    printf("%u.%u.%u.%u", ip4_addr1(ip), ip4_addr2(ip), ip4_addr3(ip), ip4_addr4(ip));
}
static inline struct netif* get_nif(void) { return netif_default ? netif_default : netif_list; }
static inline bool have_ip_up(void) {
    struct netif* nif = get_nif();
    return (nif && netif_is_up(nif) && !ip4_addr_isany_val(*netif_ip4_addr(nif)));
}
#else
static inline bool have_ip_up(void) { return false; }
#endif

#if ENABLE_WIFI
static int try_wifi_auth(uint32_t auth)
{
    printf("[WiFi] próba połączenia (auth=0x%08lx) do \"%s\"...\n", (unsigned long)auth, WIFI_SSID);
    int rc = cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASS, auth, 20000);
    if (rc) { printf("[WiFi] NIE połączono (rc=%d)\n", rc); return rc; }

    struct netif* nif = get_nif();
    if (!nif || !netif_is_up(nif)) { printf("[WiFi] interfejs nie jest UP\n"); return -1; }

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
    if (cyw43_arch_init_with_country(CYW43_COUNTRY_POLAND)) { printf("[BOOT] cyw43 init FAILED\n"); return false; }
    cyw43_arch_enable_sta_mode();
    const uint32_t try_auths[] = { CYW43_AUTH_WPA2_AES_PSK, CYW43_AUTH_WPA2_MIXED_PSK, CYW43_AUTH_WPA_TKIP_PSK, CYW43_AUTH_OPEN };
    for (size_t i = 0; i < sizeof(try_auths)/sizeof(try_auths[0]); ++i) { if (try_wifi_auth(try_auths[i]) == 0) return true; sleep_ms(500); }
    printf("[WiFi] Nie udało się połączyć z \"%s\" – sprawdź hasło/SSID.\n", WIFI_SSID);
    return false;
}
static void wifi_status_print_once(void) {
    int st = cyw43_wifi_link_status(&cyw43_state, CYW43_ITF_STA);
    const char* s="UNK"; switch (st) {
        case CYW43_LINK_DOWN:s="DOWN";break; case CYW43_LINK_JOIN:s="JOIN";break; case CYW43_LINK_NOIP:s="NOIP";break;
        case CYW43_LINK_UP:s="UP";break; case CYW43_LINK_FAIL:s="FAIL";break; case CYW43_LINK_NONET:s="NONET";break; case CYW43_LINK_BADAUTH:s="BADAUTH";break;
    }
    struct netif* nif = get_nif(); printf("[WiFi] %s  IP=", s);
    if (nif && netif_is_up(nif)) { if (!ip4_addr_isany_val(*netif_ip4_addr(nif))) print_ip4(netif_ip4_addr(nif)); else printf("0.0.0.0"); }
    else printf("0.0.0.0"); printf("\n");
}
#else
static inline bool wifi_connect_and_log(void) { return false; }
static inline void wifi_status_print_once(void) {}
#endif

// ───────────── HTTP/Sync ─────────────
static uint32_t last_http_get  = 0;
static uint32_t last_http_post = 0;
static bool     http_target_logged = false;

static float g_last_backend_set_c = NAN;  // ostatni znany setpoint z backendu
static bool  g_have_backend_cache = false;

// mechanizm ochrony lokalnej zmiany
static bool     local_override_active = false;
static float    local_override_value  = NAN;
static uint32_t local_override_until_ms = 0;

// jeżeli nie mamy endpointu SET – wyślij od razu reading z setpointem
static volatile float g_pending_setpoint = NAN;

static inline bool nearly_equal(float a, float b, float eps) { return fabsf(a - b) <= eps; }

// hooki UI
__attribute__((weak)) void  main_screen_set_target_c(float c) { (void)c; }
__attribute__((weak)) float main_screen_get_target_c(void)     { return 0.0f; }

// pomocnicze: natychmiastowy POST reading (gdy brak endpointu SET)
static void send_reading_now(void) {
#if ENABLE_WIFI
    if (!have_ip_up()) return;
    struct bme280_data d;
    if (bme280_read_data(&d) != 0) return;
    float t  = d.temperature;
    float rh = d.humidity;
    float p  = d.pressure / 100.0f;
    float set_c = isnan(g_pending_setpoint) ? main_screen_get_target_c() : g_pending_setpoint;
    hb_http_status_t pst = hb_http_post_reading(HB_HOST, (uint16_t)HB_PORT, HB_DEVICE_ID, t, rh, p, set_c, 3000);
    if (pst != HB_HTTP_OK) printf("[NET] POST (immediate) failed: %d\n", (int)pst);
#endif
}

// wywoływane z UI (slider/suwak/arc)
void heatbeat_on_target_temp_changed(float new_target) {
    // 1) włącz ochronę lokalnej zmiany
    local_override_active = true;
    local_override_value  = new_target;
    local_override_until_ms = to_ms_since_boot(get_absolute_time()) + 7000; // okno 7s

#ifdef HB_HAVE_SET_ENDPOINT
    // 2a) bezpośredni SET – natychmiast spróbuj ustawić
    if (have_ip_up()) {
        hb_http_status_t s = hb_http_set_settings_target_temp(HB_HOST, (uint16_t)HB_PORT, HB_DEVICE_ID, new_target, 3000);
        if (s != HB_HTTP_OK) { g_pending_setpoint = new_target; send_reading_now(); }
    } else {
        g_pending_setpoint = new_target;
    }
#else
    // 2b) brak endpointu – wyślij od razu reading z nowym setpointem
    g_pending_setpoint = new_target;
    send_reading_now();
#endif
}

// ───────────── MAIN ─────────────
int main(void) {
    gpio_init(BOOT_DIAG_LED); gpio_set_dir(BOOT_DIAG_LED, GPIO_OUT);
    for (int i=0;i<3;i++){ gpio_put(BOOT_DIAG_LED,1); sleep_ms(80); gpio_put(BOOT_DIAG_LED,0); sleep_ms(80); }

    stdio_usb_init();
    absolute_time_t t_limit = make_timeout_time_ms(3000);
    while (!stdio_usb_connected() && absolute_time_diff_us(get_absolute_time(), t_limit) > 0) sleep_ms(50);

    printf("\r\n=== HeatBeat-Pico start ===\r\n");
    print_free_ram("Boot");

    bool wifi_ok = false;
#if ENABLE_WIFI
    {
        absolute_time_t wifi_deadline = make_timeout_time_ms(5000);
        printf("[BOOT] Start Wi-Fi init...\n");
        while (absolute_time_diff_us(get_absolute_time(), wifi_deadline) > 0) { wifi_ok = wifi_connect_and_log(); if (wifi_ok) break; sleep_ms(250); }
        if (!wifi_ok) printf("[BOOT] Wi-Fi SAFE-MODE: UI rusza bez sieci.\n");
    }
#endif

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

    uint32_t last_read = to_ms_since_boot(get_absolute_time());
    uint32_t last_time = last_read;
    uint32_t last_bme_print = last_read;

#if ENABLE_WIFI
    uint32_t last_wifi_status_print = last_read;
    int last_status = -999;
    uint32_t last_ip_raw = 0;
#endif

    struct tm now_tm;
    struct bme280_data bme_data;

    printf("[BOOT] Main loop.\n");
    while (true) {
        lv_timer_handler();
        sleep_ms(LVGL_TICK_MS);

        uint32_t now = to_ms_since_boot(get_absolute_time());

        // zegar i heartbeat
        if (now - last_time > 1000) {
            bsp_pcf85063_get_time(&now_tm);
            char buf[32]; snprintf(buf, sizeof(buf), "%02d:%02d:%02d", now_tm.tm_hour, now_tm.tm_min, now_tm.tm_sec);
            if (label_time) lv_label_set_text(label_time, buf);
            last_time = now;
            gpio_put(BOOT_DIAG_LED, (now/1000) & 1);
        }

        // BME co 2 s
        if (now - last_read > 2000) {
            if (bme280_read_data(&bme_data) == 0) {
                extern float current_temp; extern int humidity; extern float pressure;
                current_temp = bme_data.temperature;
                humidity     = (int)(bme_data.humidity + 0.5f);
                pressure     = bme_data.pressure;
                extern void update_labels(void); update_labels();

                if (now - last_bme_print > 10000) {
                    printf("[BME] T=%.2f°C RH=%d%% P=%.2f hPa\n", current_temp, humidity, pressure/100.0f);
                    last_bme_print = now;
                }
            }
            last_read = now;
        }

#if ENABLE_WIFI
        if (wifi_ok && (now - last_wifi_status_print > 10000)) {
            int st = cyw43_wifi_link_status(&cyw43_state, CYW43_ITF_STA);
            struct netif* nif = get_nif();
            uint32_t ip_raw = (nif && netif_is_up(nif)) ? netif_ip4_addr(nif)->addr : 0;
            if (st != last_status || ip_raw != last_ip_raw) { wifi_status_print_once(); last_status = st; last_ip_raw = ip_raw; }
            else { wifi_status_print_once(); }
            last_wifi_status_print = now;
        }

        if (wifi_ok && have_ip_up()) {
            // GET settings co 10 s
            if (now - last_http_get > 10000) {
                if (!http_target_logged) { http_target_logged = true; printf("[NET] target %s:%u dev=%d\n", HB_HOST, (unsigned)HB_PORT, HB_DEVICE_ID); }
                float target_c = 0.0f;
                hb_http_status_t gst = hb_http_get_settings_target_temp(HB_HOST, (uint16_t)HB_PORT, HB_DEVICE_ID, &target_c, 4000);
                if (gst == HB_HTTP_OK) {
                    float ui_now = main_screen_get_target_c();

                    // 1) Aktywna lokalna zmiana i nie minęło okno → NIE NADPISUJ (chyba że backend == lokalna)
                    if (local_override_active && now < local_override_until_ms) {
                        if (nearly_equal(target_c, local_override_value, 0.05f)) {
                            local_override_active = false; // potwierdzone
                            g_last_backend_set_c = target_c; g_have_backend_cache = true;
                            printf("[SYNC] backend potwierdził %.2f°C\n", target_c);
                        } else {
                            printf("[SYNC] ignoruję GET (backend=%.2f, local=%.2f) do %ums\n",
                                   target_c, local_override_value, (unsigned)(local_override_until_ms - now));
                        }
                    }
                    // 2) Brak lokalnego override → normalnie, ale nie nadpisuj jeśli UI wyraźnie różne i świeżo ustawione
                    else {
                        // jeżeli róźnica > 0.05°C i UI ≠ backend, a różnica nie wynika z rounding łuku,
                        // stosuj prostą heurytykę: jeśli UI różni się > 0.05°C, to przyjmij backend
                        // tylko jeśli od ostatniej lokalnej zmiany minęło okno (tu już minęło), w przeciwnym razie – i tak UI=źródło.
                        if (!nearly_equal(target_c, ui_now, 0.05f)) {
                            // tu minęło okno albo nie było lokalnej zmiany → przyjmij backend
                            g_last_backend_set_c = target_c; g_have_backend_cache = true;
                            main_screen_set_target_c(target_c);
                            printf("[NET] new target from backend: %.2f C (override=off, ui=%.2f)\n", target_c, ui_now);
                        } else {
                            // to samo co w UI — tylko aktualizuj cache
                            g_last_backend_set_c = target_c; g_have_backend_cache = true;
                            printf("[NET] backend=UI=%.2f C (no-op)\n", target_c);
                        }
                    }
                } else {
                    printf("[NET] GET /device/%d/settings failed: %d\n", HB_DEVICE_ID, (int)gst);
                }
                last_http_get = now;
            }

            // cykliczny POST co 5 s
            if (now - last_http_post > 5000) {
                if (!isnan(g_pending_setpoint)) {
                    send_reading_now();
                    g_pending_setpoint = NAN;
                } else {
                    struct bme280_data d;
                    if (bme280_read_data(&d) == 0) {
                        float t  = d.temperature;
                        float rh = d.humidity;
                        float p  = d.pressure / 100.0f;
                        float set_c = main_screen_get_target_c();
                        hb_http_status_t pst = hb_http_post_reading(HB_HOST, (uint16_t)HB_PORT, HB_DEVICE_ID, t, rh, p, set_c, 3000);
                        if (pst != HB_HTTP_OK) printf("[NET] POST /device/%d/reading failed: %d\n", HB_DEVICE_ID, (int)pst);
                    }
                }
                last_http_post = now;
            }

            // wygaszenie override po czasie
            if (local_override_active && now >= local_override_until_ms) {
                printf("[SYNC] override timeout — wracam do normalnej synchronizacji\n");
                local_override_active = false;
            }
        }
#endif
    }
}
