#include <stdio.h>
#include <stdint.h>
#include <time.h>
#include "pico/stdlib.h"
#include "pico/stdio_usb.h"
#include "pico/time.h"   // absolute_time_t, repeating_timer
#include "hardware/gpio.h"

#include "lvgl.h"
#include "bme280_port.h"
#include "../lv_port/lv_port_disp.h"
#include "../lv_port/lv_port_indev.h"
#include "bsp_i2c.h"
#include "bsp_pcf85063.h"
#include "lvgl_ui/screen/main_screen.h"
#include "net/hb_http.h"

#ifndef ENABLE_WIFI
#define ENABLE_WIFI 1
#endif

#ifndef ENABLE_HTTP_CLIENT
#define ENABLE_HTTP_CLIENT 0
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

#define LVGL_TICK_MS 5
#define DISP_HOR_RES 466
#define DISP_VER_RES 466

#ifndef WIFI_SSID
#define WIFI_SSID "KAMNET_8960"
#endif
#ifndef WIFI_PASS
#define WIFI_PASS "Pawianywchodzanasciany"
#endif

#ifndef HEATBEAT_API_BASE
#define HEATBEAT_API_BASE "http://192.168.55.117:8000"
#endif

// LED do diagnostyki (na Pico W/2W pod cyw43, ale mamy też GPIO25)
#ifndef BOOT_DIAG_LED
#define BOOT_DIAG_LED 25
#endif

// LVGL tick timer
static bool tick_cb(struct repeating_timer *t) {
    (void)t;
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

#if ENABLE_WIFI
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
    
    // Dodatkowa stabilizacja po udanym połączeniu
    sleep_ms(200);
    
    return 0;
}
static bool wifi_connect_and_log(void) {
    printf("[BOOT] Inicjalizacja CYW43...\n");
    // Jeśli to tu wisi: najczęściej złe piny / zły port. Na Pico W/2W nie nadpisuj pinów!
    if (cyw43_arch_init_with_country(CYW43_COUNTRY_POLAND)) {
        printf("[BOOT] cyw43_arch_init_with_country() FAILED\n");
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
static void wifi_status_print_once(void) {
    int st = cyw43_wifi_link_status(&cyw43_state, CYW43_ITF_STA);
    const char* s="UNK"; switch (st) {
        case CYW43_LINK_DOWN:s="DOWN";break; case CYW43_LINK_JOIN:s="JOIN";break; case CYW43_LINK_NOIP:s="NOIP";break;
        case CYW43_LINK_UP:s="UP";break; case CYW43_LINK_FAIL:s="FAIL";break; case CYW43_LINK_NONET:s="NONET";break; case CYW43_LINK_BADAUTH:s="BADAUTH";break;
    }
    struct netif* nif = get_nif(); printf("[WiFi] %s  IP=", s);
    if (nif && netif_is_up(nif)) { 
        if (!ip4_addr_isany_val(*netif_ip4_addr(nif))) 
            print_ip4(netif_ip4_addr(nif)); 
        else 
            printf("0.0.0.0"); 
    } else {
        printf("0.0.0.0"); 
    }
    printf("\n");
}
#else
static inline bool wifi_connect_and_log(void) { return false; }
static inline void wifi_status_print_once(void) {}
#endif // ENABLE_WIFI

// Funkcja wywoływana przez UI gdy user zmieni temperaturę zadaną
void heatbeat_on_target_temp_changed(float new_target) {
    printf("[UI->HTTP] Temperatura zadana zmieniona na %.1f°C\n", new_target);
    
#if ENABLE_WIFI && ENABLE_HTTP_CLIENT
    if (!have_ip_up()) {
        printf("[HTTP] Brak połączenia WiFi - pominięcie wysyłki\n");
        return;
    }
    
    // Wysłanie do backendu - używamy stałych z CMake
    hb_http_status_t status = hb_http_set_settings_target_temp(
        "192.168.55.119",  // HB_DEFAULT_HOST
        8000,              // HB_DEFAULT_PORT  
        1,                 // HB_DEFAULT_DEVICE_ID
        new_target,
        500                // 500ms timeout (krótki żeby nie blokować UI)
    );
    
    if (status == HB_HTTP_OK) {
        printf("[HTTP] ✅ Temperatura zadana wysłana do backendu\n");
        main_screen_show_status("Wysłano do serwera", false);
    } else {
        printf("[HTTP] ❌ Błąd wysyłania (status=%d)\n", status);
        main_screen_show_status("Błąd wysyłania", true);
    }
#else
    printf("[HTTP] HTTP client wyłączony - nie wysyłam\n");
#endif
}

int main(void) {
    // Prosty „blink” diagnostyczny zanim wstanie USB/Wi-Fi
    gpio_init(BOOT_DIAG_LED);
    gpio_set_dir(BOOT_DIAG_LED, GPIO_OUT);
    for (int i=0;i<3;i++){ gpio_put(BOOT_DIAG_LED,1); sleep_ms(80); gpio_put(BOOT_DIAG_LED,0); sleep_ms(80); }

    stdio_usb_init();

    absolute_time_t t_limit = make_timeout_time_ms(3000);
    while (!stdio_usb_connected() && absolute_time_diff_us(get_absolute_time(), t_limit) > 0) {
        sleep_ms(50);
    }

    printf("\r\n=== HeatBeat-Pico start ===\r\n");
    print_free_ram("Boot");

    // --- Wi-Fi: nie pozwól, aby boot umarł, jeśli coś pójdzie źle.
    bool wifi_ok = false;
#if ENABLE_WIFI
    {
        absolute_time_t wifi_deadline = make_timeout_time_ms(5000);
        printf("[BOOT] Start Wi-Fi init...\n");
        // próbujemy aż 5 s; jeśli nie – przechodzimy dalej bez Wi-Fi
        while (absolute_time_diff_us(get_absolute_time(), wifi_deadline) > 0) {
            wifi_ok = wifi_connect_and_log();
            if (wifi_ok) break;
            sleep_ms(250);
        }
        if (!wifi_ok) {
            printf("[BOOT] Wi-Fi SAFE-MODE: UI rusza bez sieci.\n");
        }
    }
#else
    (void)wifi_ok;
#endif

    // Inicjalizacja UI
    printf("[BOOT] Init I2C/RTC/LVGL...\n");
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

    printf("[BOOT] ✅ System gotowy!\n\n");

    uint32_t last_read = to_ms_since_boot(get_absolute_time());
    uint32_t last_time = last_read;
    uint32_t last_bme_print = last_read;

#if ENABLE_WIFI
    uint32_t last_wifi_status_print = last_read;
    int last_status = -999;
    uint32_t last_ip_raw = 0;
    bool wifi_connected = false;  // Dodana brakująca zmienna
    int last_rssi = 0;            // Dodana brakująca zmienna
    int recovery_attempts_count = 0; // Dodana brakująca zmienna
#endif

    struct tm now_tm;
    struct bme280_data bme_data;

    while (true) {
        lv_timer_handler();
        
        // Aktualizacja timerów UI - ważne dla odblokowania
        uint32_t now = to_ms_since_boot(get_absolute_time());
        main_screen_update_timers_with_time(now);
        
        // Sprawdzenie auto-lock ekranu - ważne dla odblokowania
        check_auto_lock();
        
        sleep_ms(LVGL_TICK_MS);

        // zegar i heartbeat
        if (now - last_time > 1000) {
            bsp_pcf85063_get_time(&now_tm);
            char buf[32]; snprintf(buf, sizeof(buf), "%02d:%02d:%02d", now_tm.tm_hour, now_tm.tm_min, now_tm.tm_sec);
            if (label_time) lv_label_set_text(label_time, buf);
            last_time = now;
            // mały heartbeat na LED co sekundę
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
            bool connected = have_ip_up();
            
            if (st != last_status || ip_raw != last_ip_raw || connected != wifi_connected) {
                wifi_status_print_once();
                wifi_connected = connected;
                
                if (connected) {
                    last_rssi = cyw43_wifi_get_rssi(&cyw43_state, CYW43_ITF_STA);
                    main_screen_update_wifi_status(true, last_rssi);
                    
                    // Reset recovery counter przy udanym połączeniu
                    if (recovery_attempts_count > 0) {
                        printf("[CYW43] Połączenie przywrócone - reset recovery counter\n");
                        recovery_attempts_count = 0;
                    }
                    
                    // NIE wykonuj automatycznego sync po recovery - może nadpisać lokalne zmiany
                    // if (!g_have_backend_cache) {
                    //     startup_sync_with_server(); // WYŁĄCZONE - nie nadpisuj lokalnych ustawień
                    // }
                } else {
                    main_screen_update_wifi_status(false, 0);
                    main_screen_show_status("Utracono połączenie", true);
                }
                
                last_status = st; 
                last_ip_raw = ip_raw;
            }
            last_wifi_status_print = now;
        }
#endif
    }
}
