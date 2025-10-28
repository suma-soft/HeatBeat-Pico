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

// lwIP – NO_SYS (poll mode, bez wątków)
#include "lwip/netif.h"
#include "lwip/ip4_addr.h"

#define LVGL_TICK_MS 5
#define DISP_HOR_RES 466
#define DISP_VER_RES 466

#ifndef WIFI_SSID
#define WIFI_SSID "KAMNET_8960"
#endif
#ifndef WIFI_PASS
#define WIFI_PASS "Pawianywchodzanasciany"
#endif

static bool tick_cb(struct repeating_timer *t) { 
    lv_tick_inc(LVGL_TICK_MS); 
    return true; 
}

extern char __StackLimit, __bss_end__;
static void print_free_ram(const char* msg) {
    uint32_t free_ram = (uint32_t)&__StackLimit - (uint32_t)&__bss_end__;
    printf("[RAM] %s: Wolna RAM: %lu bajtów\n", msg, (unsigned long)free_ram);
}

static void print_ip4(const ip4_addr_t* ip) {
    printf("%u.%u.%u.%u", ip4_addr1(ip), ip4_addr2(ip), ip4_addr3(ip), ip4_addr4(ip));
}

static inline struct netif* get_nif(void) {
    return netif_default ? netif_default : netif_list;
}

static int try_wifi_auth(uint32_t auth)
{
    printf("WiFi: próba połączenia (auth=0x%08lx) do \"%s\"...\n", (unsigned long)auth, WIFI_SSID);
    int rc = cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASS, auth, 20000);
    if (rc) {
        printf("WiFi: NIE połączono (rc=%d)\n", rc);
        return rc;
    }

    struct netif* nif = get_nif();
    if (!nif || !netif_is_up(nif)) {
        printf("WiFi: interfejs nie jest UP\n");
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
    printf("➡️ Inicjalizacja układu CYW43 (RM2)...\n");
    printf("[DEBUG] Wywołuję cyw43_arch_init_with_country()...\n");
    
    if (cyw43_arch_init_with_country(CYW43_COUNTRY_POLAND)) {
        printf("❌ cyw43_arch_init_with_country() failed\n");
        return false;
    }
    
    printf("✅ cyw43_arch_init OK\n");
    cyw43_arch_enable_sta_mode();
    printf("✅ enable_sta_mode OK\n");

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

// ✅ Callback dla UI
void heatbeat_on_target_temp_changed(float new_target_temp) {
    printf("[UI] Zmieniono temperaturę docelową na: %.1f°C\n", new_target_temp);
}

int main(void) {
    stdio_usb_init();
    sleep_ms(2000);
    
    printf("\r\n\r\n");
    printf("===================================\n");
    printf("  HeatBeat-Pico v1.0 (RISC-V)\n");
    printf("===================================\n");
    printf("Build: %s %s\n", __DATE__, __TIME__);
    print_free_ram("Boot");

    printf("\n[1/6] Inicjalizacja WiFi...\n");
    bool wifi_ok = wifi_connect_and_log();
    if (!wifi_ok) {
        printf("⚠️  WiFi nie podłączono - kontynuuję bez sieci\n");
    }

    printf("[2/6] Inicjalizacja I2C...\n");
    bsp_i2c_init();
    bsp_pcf85063_init();

    printf("[3/6] Inicjalizacja LVGL...\n");
    lv_init();
    lv_port_disp_init(DISP_HOR_RES, DISP_VER_RES, 0, false);
    lv_port_indev_init(DISP_HOR_RES, DISP_VER_RES, 0);

    printf("[4/6] Inicjalizacja BME280...\n");
    bme280_init_default();

    printf("[5/6] Ładowanie interfejsu...\n");
    main_screen_init();
    lv_scr_load(ui_main_screen);

    static struct repeating_timer t;
    add_repeating_timer_ms(LVGL_TICK_MS, tick_cb, NULL, &t);

    printf("[6/6] ✅ System gotowy!\n\n");

    uint32_t last_read = to_ms_since_boot(get_absolute_time());
    uint32_t last_time = last_read;
    uint32_t last_bme_print = last_read;
    uint32_t last_wifi_status_print = last_read;

    int last_status = -999;
    uint32_t last_ip_raw = 0;

    struct tm now_tm;
    struct bme280_data bme_data;

    while (true) {
        cyw43_arch_poll();  // ✅ KLUCZOWE dla NO_SYS
        
        lv_timer_handler();
        sleep_ms(LVGL_TICK_MS);

        uint32_t now = to_ms_since_boot(get_absolute_time());

        if (now - last_time > 1000) {
            bsp_pcf85063_get_time(&now_tm);
            char buf[32];
            snprintf(buf, sizeof(buf), "%02d:%02d:%02d", now_tm.tm_hour, now_tm.tm_min, now_tm.tm_sec);
            if (label_time) lv_label_set_text(label_time, buf);
            last_time = now;
        }

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

                if (now - last_bme_print > 10000) {
                    printf("[BME] T=%.2f°C RH=%d%% P=%.2f hPa\n",
                           current_temp, humidity, pressure / 100.0f);
                    last_bme_print = now;
                }
            }
            last_read = now;
        }

        if (now - last_wifi_status_print > 10000) {
            int st = cyw43_wifi_link_status(&cyw43_state, CYW43_ITF_STA);
            struct netif* nif = get_nif();
            uint32_t ip_raw = (nif && netif_is_up(nif)) ? netif_ip4_addr(nif)->addr : 0;

            if (st != last_status || ip_raw != last_ip_raw) {
                wifi_status_print_once();
                last_status = st;
                last_ip_raw = ip_raw;
            }
            last_wifi_status_print = now;
        }
    }
}