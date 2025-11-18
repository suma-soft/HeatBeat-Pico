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
#include "lvgl_ui/screen/main_screen.h"

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
#define HB_HOST "192.168.55.119"
#endif
#ifndef HB_PORT
#define HB_PORT 8000
#endif
#ifndef HB_DEVICE_ID
#define HB_DEVICE_ID 1
#endif

/* Je┼╝eli masz endpoint ustawie┼ä (POST/PUT /device/{id}/settings),
 * w┼é─ůcz i zaimplementuj hb_http_set_settings_target_temp():
 */
// #define HB_HAVE_SET_ENDPOINT 1

#ifndef BOOT_DIAG_LED
#define BOOT_DIAG_LED 25
#endif

static bool tick_cb(struct repeating_timer *t) { (void)t; lv_tick_inc(LVGL_TICK_MS); return true; }

extern char __StackLimit, __bss_end__;
static void print_free_ram(const char* msg) {
    uint32_t free_ram = (uint32_t)&__StackLimit - (uint32_t)&__bss_end__;
    printf("[RAM] %s: Wolna RAM: %lu bajt├│w\n", msg, (unsigned long)free_ram);
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
    printf("[WiFi] pr├│ba po┼é─ůczenia (auth=0x%08lx) do \"%s\"...\n", (unsigned long)auth, WIFI_SSID);
    
    // Dodatkowa stabilizacja przed pr├│b─ů po┼é─ůczenia
    sleep_ms(100);
    
    int rc = cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASS, auth, 25000); // Zwi─Ökszony timeout
    if (rc) { 
        printf("[WiFi] NIE po┼é─ůczono (rc=%d)\n", rc); 
        
        // Sprawd┼║ czy to b┼é─ůd CYW43
        if (rc == PICO_ERROR_GENERIC || rc == PICO_ERROR_TIMEOUT) {
            printf("[WiFi] Mo┼╝liwy b┼é─ůd komunikacji z CYW43\n");
            sleep_ms(500); // Dodatkowe op├│┼║nienie przy b┼é─Ödach komunikacji
        }
        return rc; 
    }

    struct netif* nif = get_nif();
    if (!nif || !netif_is_up(nif)) { 
        printf("[WiFi] interfejs nie jest UP\n"); 
        return -1; 
    }

    printf("[WiFi] Po┼é─ůczono z \"%s\"  IP:", WIFI_SSID);
    print_ip4(netif_ip4_addr(nif));
    printf("  GW: "); print_ip4(netif_ip4_gw(nif));
    printf("  MASK: "); print_ip4(netif_ip4_netmask(nif));
    printf("\n");

    int rssi = cyw43_wifi_get_rssi(&cyw43_state, CYW43_ITF_STA);
    if (rssi != 0) printf("[WiFi] RSSI: %d dBm\n", rssi);
    
    // Dodatkowa stabilizacja po udanym po┼é─ůczeniu
    sleep_ms(200);
    
    return 0;
}
static bool wifi_connect_and_log(void) {
    printf("[BOOT] Inicjalizacja CYW43...\n");
    
    int init_result = cyw43_arch_init_with_country(CYW43_COUNTRY_POLAND);
    if (init_result != 0) {
        printf("[CYW43] B┼é─ůd inicjalizacji: %d\n", init_result);
        return false;
    }
    
    printf("[CYW43] Inicjalizacja pomy┼Ťlna\n");
    
    // Stabilizacja po inicjalizacji
    sleep_ms(500);
    
    cyw43_arch_enable_sta_mode();
    
    // Dodatkowa stabilizacja po w┼é─ůczeniu trybu STA
    sleep_ms(200);
    
    const uint32_t try_auths[] = { CYW43_AUTH_WPA2_AES_PSK, CYW43_AUTH_WPA2_MIXED_PSK, CYW43_AUTH_WPA_TKIP_PSK, CYW43_AUTH_OPEN };
    for (size_t i = 0; i < sizeof(try_auths)/sizeof(try_auths[0]); ++i) { 
        if (try_wifi_auth(try_auths[i]) == 0) return true; 
        sleep_ms(1000); // Zwi─Ökszone op├│┼║nienie mi─Ödzy pr├│bami
    }
    printf("[WiFi] Nie uda┼éo si─Ö po┼é─ůczy─ç z \"%s\" ÔÇô sprawd┼║ has┼éo/SSID.\n", WIFI_SSID);
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
#endif

// Funkcja do obs┼éugi b┼é─Öd├│w CYW43 i recovery
static bool cyw43_error_recovery(void) {
#if ENABLE_WIFI
    printf("[CYW43] Wykryto b┼é─ůd - pr├│ba recovery...\n");
    
    // Spr├│buj mi─Ökki reset
    cyw43_arch_deinit();
    sleep_ms(2000); // D┼éu┼╝sza pauza dla stabilno┼Ťci
    
    // Ponowna inicjalizacja
    if (cyw43_arch_init_with_country(CYW43_COUNTRY_POLAND) == 0) {
        printf("[CYW43] Recovery pomy┼Ťlne - pr├│ba ponownego po┼é─ůczenia\n");
        cyw43_arch_enable_sta_mode();
        sleep_ms(500);
        
        // Spr├│buj ponownie po┼é─ůczy─ç si─Ö z WiFi
        const uint32_t try_auths[] = { CYW43_AUTH_WPA2_AES_PSK, CYW43_AUTH_WPA2_MIXED_PSK, CYW43_AUTH_WPA_TKIP_PSK };
        for (size_t i = 0; i < sizeof(try_auths)/sizeof(try_auths[0]); ++i) { 
            if (try_wifi_auth(try_auths[i]) == 0) {
                printf("[CYW43] Recovery + reconnect pomy┼Ťlne\n");
                return true;
            }
            sleep_ms(1000);
        }
        
        printf("[CYW43] Recovery udane, ale nie uda┼éo si─Ö po┼é─ůczy─ç z WiFi\n");
        return false;
    } else {
        printf("[CYW43] Recovery nieudane\n");
        return false;
    }
#else
    return false;
#endif
}

// Funkcja do sprawdzania stanu CYW43
static bool check_cyw43_health(void) {
#if ENABLE_WIFI
    // Sprawd┼║ podstawowe funkcje CYW43
    int link_status = cyw43_wifi_link_status(&cyw43_state, CYW43_ITF_STA);
    
    // Sprawd┼║ czy status jest w dozwolonym zakresie
    // Status 0 (DOWN) jest OK gdy nie ma po┼é─ůczenia
    if (link_status < CYW43_LINK_DOWN || link_status > CYW43_LINK_BADAUTH) {
        printf("[CYW43] Nieprawid┼éowy status: %d\n", link_status);
        return false;
    }
    
    // Je┼Ťli mamy po┼é─ůczenie IP, ale link status to DOWN - problem
    if (have_ip_up() && link_status == CYW43_LINK_DOWN) {
        printf("[CYW43] Niesp├│jno┼Ť─ç: mamy IP ale link DOWN\n");
        return false;
    }
    
    // Je┼Ťli brak IP przez d┼éugi czas i status pokazuje link up - mo┼╝e by─ç problem
    static int link_up_without_ip_count = 0;
    if (link_status == CYW43_LINK_UP && !have_ip_up()) {
        link_up_without_ip_count++;
        if (link_up_without_ip_count > 5) { // 5 sprawdze┼ä = 2.5 minuty
            printf("[CYW43] Link UP ale brak IP przez d┼éugi czas\n");
            link_up_without_ip_count = 0;
            return false;
        }
    } else {
        link_up_without_ip_count = 0;
    }
    
    // Test komunikacji przez RSSI tylko gdy jest po┼é─ůczenie
    static int consecutive_errors = 0;
    if (have_ip_up()) {
        int rssi = cyw43_wifi_get_rssi(&cyw43_state, CYW43_ITF_STA);
        if (rssi == 0) {
            // RSSI = 0 przy aktywnym po┼é─ůczeniu mo┼╝e wskazywa─ç na problem
            consecutive_errors++;
            if (consecutive_errors > 3) {
                printf("[CYW43] Wielokrotne b┼é─Ödy RSSI, mo┼╝liwy problem komunikacji\n");
                consecutive_errors = 0;
                return false;
            }
        } else {
            consecutive_errors = 0;
        }
    }
    
    return true;
#else
    return true;
#endif
}

// Funkcja do wykrywania b┼é─Öd├│w hdr mismatch
static void cyw43_monitor_errors(void) {
#if ENABLE_WIFI
    // Tutaj mo┼╝na doda─ç monitoring specific errors
    // Na razie bazowy monitoring w check_cyw43_health
#endif
}

// ÔöÇÔöÇÔöÇÔöÇÔöÇÔöÇÔöÇÔöÇÔöÇÔöÇÔöÇÔöÇÔöÇ HTTP/Sync ÔöÇÔöÇÔöÇÔöÇÔöÇÔöÇÔöÇÔöÇÔöÇÔöÇÔöÇÔöÇÔöÇ
static uint32_t last_http_post = 0;
static bool     http_target_logged = false;

// Cache backendu
static float g_last_backend_set_c = NAN;  
static char  g_last_backend_source[16] = "";
static bool  g_have_backend_cache = false;

// mechanizm ochrony lokalnej zmiany
static bool     local_override_active = false;
static float    local_override_value  = NAN;
static uint32_t local_override_until_ms = 0;
static uint32_t local_override_start_ms = 0;

// pending operations
static volatile float g_pending_setpoint = NAN;
static volatile bool  g_pending_get_request = false;

// Status po┼é─ůczenia
static bool wifi_connected = false;
static int  last_rssi = 0;
static uint32_t last_recovery_attempt = 0;
static int recovery_attempts_count = 0;

// Parametry konfiguracyjne
#define SERVER_CHECK_INTERVAL_MS    5000   // Sprawdzanie serwera co 5s (dla test├│w)
#define READING_SEND_INTERVAL_MS    15000  // Wysy┼éanie odczyt├│w co 15s
#define LOCAL_OVERRIDE_WINDOW_MS    5000   // Okno ochrony lokalnej zmiany
#define CONNECTION_TIMEOUT_MS       4000   // Timeout dla po┼é─ůcze┼ä HTTP
#define HEARTBEAT_INTERVAL_MS       30000  // Heartbeat co 30s

// Helper function for floating point comparison
static inline bool nearly_equal(float a, float b, float eps) { 
    return fabsf(a - b) <= eps; 
}

// hooki UI
__attribute__((weak)) void  main_screen_set_target_c(float c) { (void)c; }
__attribute__((weak)) float main_screen_get_target_c(void)     { return 0.0f; }
__attribute__((weak)) void  main_screen_show_status(const char *message, bool is_error) { (void)message; (void)is_error; }
__attribute__((weak)) void  main_screen_show_notification(const char *message, int duration_ms) { (void)message; (void)duration_ms; }
__attribute__((weak)) void  main_screen_update_wifi_status(bool connected, int rssi) { (void)connected; (void)rssi; }
__attribute__((weak)) void  main_screen_set_target_c_from_server(float c, const char *source) { (void)c; (void)source; }
__attribute__((weak)) void  main_screen_update_timers_with_time(uint32_t now) { (void)now; }
__attribute__((weak)) void  main_screen_set_notification_time(uint32_t time) { (void)time; }

// Startup - pobierz ustawienia z serwera
static void startup_sync_with_server(void) {
    if (!have_ip_up()) {
        main_screen_show_status("Brak po┼é─ůczenia z serwerem", true);
        printf("[STARTUP] Brak po┼é─ůczenia IP - pomijam sync\n");
        return;
    }
    
    // Nie nadpisuj aktywnych lokalnych zmian
    uint32_t now = to_ms_since_boot(get_absolute_time());
    if (local_override_active && now < local_override_until_ms) {
        printf("[STARTUP] Pomijam sync - aktywna lokalna zmiana (do %ums)\n", 
               (unsigned)local_override_until_ms);
        main_screen_show_status("Zachowuj─Ö lokalne ustawienia", false);
        return;
    }
    
    printf("[STARTUP] Rozpoczynam synchronizacj─Ö z serwerem...\n");
    main_screen_show_status("┼ü─ůczenie z serwerem...", false);
    print_free_ram("Przed HTTP");
    
    // Dodatkowe op├│┼║nienie dla stabilno┼Ťci po┼é─ůczenia
    sleep_ms(1000);
    
    printf("[STARTUP] Wysy┼éam ┼╝─ůdanie GET do %s:%d\n", HB_HOST, HB_PORT);
    
    hb_settings_response_t settings;
    hb_http_status_t st = hb_http_get_settings(HB_HOST, (uint16_t)HB_PORT, HB_DEVICE_ID, 
                                              &settings, CONNECTION_TIMEOUT_MS);
    
    print_free_ram("Po HTTP");
    printf("[STARTUP] Odpowied┼║ z serwera: status=%d\n", (int)st);
    
    if (st == HB_HTTP_OK) {
        printf("[STARTUP] HTTP OK - rozpoczynam parsowanie...\n");
        printf("[STARTUP] settings.target_temp_c = %.1f\n", settings.target_temp_c);
        printf("[STARTUP] settings.last_source = '%s'\n", settings.last_source);
        
        g_last_backend_set_c = settings.target_temp_c;
        printf("[STARTUP] Temp zapisana: %.1f\n", g_last_backend_set_c);
        
        strncpy(g_last_backend_source, settings.last_source, sizeof(g_last_backend_source) - 1);
        g_last_backend_source[sizeof(g_last_backend_source) - 1] = '\0';
        printf("[STARTUP] Source zapisane: '%s'\n", g_last_backend_source);
        
        g_have_backend_cache = true;
        printf("[STARTUP] Cache ustawiony\n");
        
        // TYMCZASOWO WY┼ü─äCZONE - UI b─Ödzie aktualizowane p├│┼║niej w main loop
        // printf("[STARTUP] Wywo┼éuj─Ö main_screen_set_target_c...\n");
        // main_screen_set_target_c(settings.target_temp_c);
        // printf("[STARTUP] main_screen_set_target_c zako┼äczone\n");
        
        printf("[STARTUP] Wywo┼éuj─Ö main_screen_show_status...\n");
        main_screen_show_status("Po┼é─ůczono z serwerem", false);
        printf("[STARTUP] main_screen_show_status zako┼äczone\n");
        
        printf("[STARTUP] Pobrano ustawienia: temp=%.1f┬░C, source='%s'\n", 
               settings.target_temp_c, settings.last_source);
    } else {
        main_screen_show_status("B┼é─ůd komunikacji - pracuj─Ö offline", true);
        printf("[STARTUP] B┼é─ůd pobierania ustawie┼ä: %d\n", (int)st);
    }
    
    printf("[STARTUP] Synchronizacja zako┼äczona\n");
    printf("[STARTUP] Oczekiwanie 5 sekund przed kontynuacj─ů...\n");
    sleep_ms(5000);
    printf("[STARTUP] Kontynuacja po op├│┼║nieniu\n");
}

// Natychmiastowe wys┼éanie odczytu z pending setpoint
static void send_reading_now(void) {
#if ENABLE_WIFI
    if (!have_ip_up()) {
        main_screen_show_status("Brak po┼é─ůczenia z serwerem", true);
        return;
    }
    
    struct bme280_data d;
    if (bme280_read_data(&d) != 0) return;
    
    float t  = d.temperature;
    float rh = d.humidity;
    float p  = d.pressure / 100.0f;
    float set_c = isnan(g_pending_setpoint) ? main_screen_get_target_c() : g_pending_setpoint;
    
    hb_http_status_t pst = hb_http_post_reading(HB_HOST, (uint16_t)HB_PORT, HB_DEVICE_ID, 
                                               t, rh, p, set_c, CONNECTION_TIMEOUT_MS);
    
    if (pst == HB_HTTP_OK) {
        main_screen_show_notification("Wys┼éano do aplikacji", 2000);
        main_screen_set_notification_time(to_ms_since_boot(get_absolute_time()) + 2000);
        printf("[NET] POST reading: temp=%.1f┬░C, setpoint=%.1f┬░C\n", t, set_c);
    } else {
        main_screen_show_status("Nie uda┼éo si─Ö wys┼éa─ç danych", true);
        printf("[NET] POST reading failed: %d\n", (int)pst);
    }
#endif
}

// Pr├│ba bezpo┼Ťredniego ustawienia temperatury przez PUT
static bool try_direct_set_temperature(float new_target) {
    if (!have_ip_up()) return false;
    
    hb_http_status_t st = hb_http_set_settings_target_temp(HB_HOST, (uint16_t)HB_PORT, 
                                                          HB_DEVICE_ID, new_target, 
                                                          CONNECTION_TIMEOUT_MS);
    if (st == HB_HTTP_OK) {
        main_screen_show_notification("Temperatura zaktualizowana", 2000);
        main_screen_set_notification_time(to_ms_since_boot(get_absolute_time()) + 2000);
        printf("[NET] PUT settings: target=%.1f┬░C\n", new_target);
        return true;
    } else {
        printf("[NET] PUT settings failed: %d\n", (int)st);
        return false;
    }
}

// wywo┼éywane z UI (slider/suwak/arc)
void heatbeat_on_target_temp_changed(float new_target) {
    uint32_t now = to_ms_since_boot(get_absolute_time());
    
    // 1) Aktywuj ochron─Ö lokalnej zmiany
    local_override_active = true;
    local_override_value  = new_target;
    local_override_until_ms = now + LOCAL_OVERRIDE_WINDOW_MS;
    local_override_start_ms = now;
    
    printf("[LOCAL] Zmiana temperatury: %.1f┬░C (ochrona do %ums)\n", 
           new_target, (unsigned)local_override_until_ms);

    // 2) Spr├│buj bezpo┼Ťredniego ustawienia (PUT endpoint)
    bool put_success = try_direct_set_temperature(new_target);
    
    // 3) Je┼Ťli PUT si─Ö nie uda┼é, zapisz jako pending i wy┼Ťlij przez reading
    if (!put_success) {
        g_pending_setpoint = new_target;
        main_screen_show_notification("Wysy┼éanie...", 1500);
        main_screen_set_notification_time(now + 1500);
        
        // Natychmiastowa pr├│ba przez POST reading
        send_reading_now();
    }
    
    // 4) Zaplanuj szybkie sprawdzenie serwera za 2s
    g_pending_get_request = true;
}

// Funkcje dodatkowe do obs┼éugi b┼é─Öd├│w i logowania
static void log_system_status(void) {
    printf("[STATUS] WiFi: %s, Backend cache: %s, Local override: %s\n", 
           wifi_connected ? "Connected" : "Disconnected",
           g_have_backend_cache ? "Yes" : "No",
           local_override_active ? "Active" : "Inactive");
    
    if (g_have_backend_cache) {
        printf("[STATUS] Backend: temp=%.1f┬░C, source='%s'\n", 
               g_last_backend_set_c, g_last_backend_source);
    }
}

// Funkcja do retry operacji
static void retry_failed_operations(void) {
    if (!isnan(g_pending_setpoint) && have_ip_up()) {
        printf("[RETRY] Ponowna pr├│ba wys┼éania pending setpoint: %.1f┬░C\n", g_pending_setpoint);
        send_reading_now();
    }
}

// ÔöÇÔöÇÔöÇÔöÇÔöÇÔöÇÔöÇÔöÇÔöÇÔöÇÔöÇÔöÇÔöÇ MAIN ÔöÇÔöÇÔöÇÔöÇÔöÇÔöÇÔöÇÔöÇÔöÇÔöÇÔöÇÔöÇÔöÇ
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
        absolute_time_t wifi_deadline = make_timeout_time_ms(20000); // Zwi─Ökszony timeout
        printf("[BOOT] Start Wi-Fi init...\n");
        main_screen_show_status("┼ü─ůczenie z WiFi...", false);
        
        int wifi_attempts = 0;
        const int max_wifi_attempts = 2;
        
        while (absolute_time_diff_us(get_absolute_time(), wifi_deadline) > 0 && wifi_attempts < max_wifi_attempts) { 
            wifi_attempts++;
            printf("[WIFI] Pr├│ba po┼é─ůczenia %d/%d\n", wifi_attempts, max_wifi_attempts);
            
            wifi_ok = wifi_connect_and_log(); 
            if (wifi_ok) {
                wifi_connected = true;
                int rssi = cyw43_wifi_get_rssi(&cyw43_state, CYW43_ITF_STA);
                main_screen_update_wifi_status(true, rssi);
                printf("[BOOT] WiFi po┼é─ůczony pomy┼Ťlnie na pr├│bie %d\n", wifi_attempts);
                break; 
            } else {
                printf("[BOOT] Pr├│ba WiFi %d nieudana\n", wifi_attempts);
                if (wifi_attempts < max_wifi_attempts) {
                    printf("[BOOT] Oczekiwanie przed kolejn─ů pr├│b─ů...\n");
                    printf("[BOOT] Deinicjalizacja CYW43 przed kolejn─ů pr├│b─ů...\n");
                    cyw43_arch_deinit();  // Wyczy┼Ť─ç stan CYW43 przed kolejn─ů pr├│b─ů
                    sleep_ms(2000);
                }
            }
        }
        
        if (!wifi_ok) {
            printf("[BOOT] Wi-Fi SAFE-MODE: UI rusza bez sieci po %d pr├│bach\n", wifi_attempts);
            main_screen_show_status("WiFi niedost─Öpne - tryb offline", true);
            main_screen_update_wifi_status(false, 0);
        } else {
            // Startup sync z serwerem - z dodatkowym op├│┼║nieniem dla stabilno┼Ťci i timeout
            printf("[BOOT] Przygotowanie do sync z serwerem...\n");
            sleep_ms(1000);
            
            // Uruchom sync w tle z timeout
            absolute_time_t sync_deadline = make_timeout_time_ms(10000); // 10s timeout
            printf("[BOOT] Rozpoczynam startup sync (timeout 10s)...\n");
            
            startup_sync_with_server();
            
            // Sprawd┼║ czy sync si─Ö zako┼äczy┼é w czasie
            if (absolute_time_diff_us(get_absolute_time(), sync_deadline) <= 0) {
                printf("[BOOT] Startup sync - timeout!\n");
                main_screen_show_status("Timeout serwera - pracuj─Ö offline", true);
            }
            
            printf("[BOOT] Startup sync zako┼äczony\n");
        }
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
    uint32_t last_server_check = 0;  // Natychmiastowe pierwsze sprawdzenie
    uint32_t last_heartbeat = last_read;

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
                    printf("[BME] T=%.2f┬░C RH=%d%% P=%.2f hPa\n", current_temp, humidity, pressure/100.0f);
                    last_bme_print = now;
                }
            }
            last_read = now;
        }

        // Aktualizacja timer├│w UI
        main_screen_update_timers_with_time(now);

        // Aktualizacja UI z cache (tylko raz po starcie)
        static bool ui_cache_applied = false;
        if (!ui_cache_applied && g_have_backend_cache) {
            printf("[UI] Aktualizuj─Ö UI z cache: temp=%.1f┬░C\n", g_last_backend_set_c);
            main_screen_set_target_c(g_last_backend_set_c);
            ui_cache_applied = true;
        }

#if ENABLE_WIFI
        // Monitoring b┼é─Öd├│w CYW43
        cyw43_monitor_errors();
        // Monitoring zdrowia CYW43 co 30 sekund
        static uint32_t last_cyw43_check = 0;
        if (wifi_ok && (now - last_cyw43_check > 30000)) {
            if (!check_cyw43_health()) {
                // Unikaj zbyt cz─Östych pr├│b recovery (minimum 2 minuty mi─Ödzy pr├│bami)
                if (now - last_recovery_attempt > 120000) {
                    printf("[CYW43] Wykryto problemy ze zdrowiem modu┼éu\n");
                    main_screen_show_status("Problem z modu┼éem WiFi", true);
                    
                    last_recovery_attempt = now;
                    recovery_attempts_count++;
                    
                    // Maksymalnie 3 pr├│by recovery na sesj─Ö
                    if (recovery_attempts_count <= 3) {
                        // Spr├│buj recovery
                        if (cyw43_error_recovery()) {
                            main_screen_show_status("WiFi przywr├│cony", false);
                            wifi_connected = have_ip_up();
                            recovery_attempts_count = 0; // Reset counter po udanym recovery
                            
                            if (wifi_connected) {
                                main_screen_update_wifi_status(true, cyw43_wifi_get_rssi(&cyw43_state, CYW43_ITF_STA));
                            } else {
                                main_screen_update_wifi_status(false, 0);
                            }
                        } else {
                            main_screen_show_status("Recovery nieudane", true);
                        }
                    } else {
                        main_screen_show_status("WiFi wy┼é─ůczony - za du┼╝o b┼é─Öd├│w", true);
                        printf("[CYW43] Wy┼é─ůczam WiFi po %d nieudanych pr├│bach recovery\n", recovery_attempts_count);
                        wifi_ok = false;
                        wifi_connected = false;
                        main_screen_update_wifi_status(false, 0);
                    }
                } else {
                    printf("[CYW43] Problemy wykryte, ale za wcze┼Ťnie na kolejny recovery\n");
                }
            }
            last_cyw43_check = now;
        }

        // Monitorowanie statusu WiFi
        if (wifi_ok && (now - last_wifi_status_print > 15000)) {
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
                    
                    // Reset recovery counter przy udanym po┼é─ůczeniu
                    if (recovery_attempts_count > 0) {
                        printf("[CYW43] Po┼é─ůczenie przywr├│cone - reset recovery counter\n");
                        recovery_attempts_count = 0;
                    }
                    
                    // NIE wykonuj automatycznego sync po recovery - mo┼╝e nadpisa─ç lokalne zmiany
                    // if (!g_have_backend_cache) {
                    //     startup_sync_with_server(); // WY┼ü─äCZONE - nie nadpisuj lokalnych ustawie┼ä
                    // }
                } else {
                    main_screen_update_wifi_status(false, 0);
                    main_screen_show_status("Utracono po┼é─ůczenie", true);
                }
                
                last_status = st; 
                last_ip_raw = ip_raw;
            }
            last_wifi_status_print = now;
        }

        // Komunikacja z serwerem
        if (wifi_connected && have_ip_up()) {
            // Sprawdzanie serwera - cz─Ö┼Ťciej przy pending request lub zgodnie z harmonogramem
            bool should_check_server = (g_pending_get_request && now - last_server_check > 2000) ||
                                      (now - last_server_check > SERVER_CHECK_INTERVAL_MS);
                                      
            if (should_check_server) {
                if (!http_target_logged) { 
                    http_target_logged = true; 
                    printf("[NET] target %s:%u dev=%d\n", HB_HOST, (unsigned)HB_PORT, HB_DEVICE_ID); 
                }
                
                hb_settings_response_t settings;
                hb_http_status_t gst = hb_http_get_settings(HB_HOST, (uint16_t)HB_PORT, HB_DEVICE_ID, 
                                                           &settings, CONNECTION_TIMEOUT_MS);
                
                if (gst == HB_HTTP_OK) {
                    float ui_now = main_screen_get_target_c();
                    bool settings_changed = !g_have_backend_cache || 
                                          !nearly_equal(settings.target_temp_c, g_last_backend_set_c, 0.05f) ||
                                          strcmp(settings.last_source, g_last_backend_source) != 0;
                    
                    if (settings_changed) {
                        printf("[NET] Nowe ustawienia: temp=%.1f┬░C, source='%s' (ui=%.1f┬░C)\n", 
                               settings.target_temp_c, settings.last_source, ui_now);
                        printf("[NET] Cache: temp=%.1f┬░C, source='%s', have_cache=%d\n",
                               g_last_backend_set_c, g_last_backend_source, g_have_backend_cache);
                    } else {
                        printf("[NET] Brak zmian: temp=%.1f┬░C, source='%s' (ui=%.1f┬░C)\n", 
                               settings.target_temp_c, settings.last_source, ui_now);
                    }

                    // Logika aktualizacji UI na podstawie source
                    if (local_override_active && now < local_override_until_ms) {
                        // W trakcie lokalnej zmiany - sprawd┼║ czy serwer potwierdzi┼é
                        if (nearly_equal(settings.target_temp_c, local_override_value, 0.05f)) {
                            local_override_active = false;
                            main_screen_show_notification("Temperatura potwierdzona", 2000);
                            main_screen_set_notification_time(now + 2000);
                            printf("[SYNC] Serwer potwierdzi┼é %.1f┬░C\n", settings.target_temp_c);
                        } else {
                            printf("[SYNC] Ignoruj─Ö GET podczas lokalnej zmiany (serwer=%.1f, local=%.1f)\n",
                                   settings.target_temp_c, local_override_value);
                        }
                    } else {
                        // Sprawd┼║ czy to zmiana z aplikacji - ma pierwsze┼ästwo ZAWSZE
                        printf("[NET] Sprawdzanie synchronizacji: source='%s', changed=%d, local_active=%d, until=%u, now=%u\n",
                               settings.last_source, settings_changed, local_override_active, 
                               (unsigned)local_override_until_ms, (unsigned)now);
                               
                        if (strcmp(settings.last_source, "app") == 0) {
                            // Sprawd┼║ czy source zmieni┼é si─Ö z 'device' na 'app' lub temperatura si─Ö r├│┼╝ni
                            bool source_changed_to_app = g_have_backend_cache && 
                                                        strcmp(g_last_backend_source, "app") != 0;
                            bool temp_differs = !nearly_equal(settings.target_temp_c, ui_now, 0.05f);
                            
                            if (source_changed_to_app || temp_differs) {
                                printf("[SYNC] Zmiana z aplikacji wykryta - source_changed=%d, temp_differs=%d\n", 
                                       source_changed_to_app, temp_differs);
                                printf("[SYNC] Anuluj lokaln─ů ochron─Ö - zmiana z aplikacji: %.1f┬░C\n", settings.target_temp_c);
                                local_override_active = false;
                                local_override_until_ms = 0;
                                main_screen_set_target_c_from_server(settings.target_temp_c, "app");
                            } else {
                                printf("[SYNC] Zmiana z aplikacji ju┼╝ zastosowana\n");
                            }
                        } else if (settings_changed) {
                            // Inne zmiany tylko je┼Ťli rzeczywi┼Ťcie si─Ö zmieni┼éy
                            if (local_override_until_ms > 0 && now < local_override_until_ms + 30000) {
                                // Daj 30s po zako┼äczeniu ochrony zanim zezwolisz na inne nadpisanie
                                printf("[SYNC] Czekam 30s po lokalnej zmianie przed nadpisaniem (serwer=%.1f, ui=%.1f)\n",
                                       settings.target_temp_c, ui_now);
                            } else if (!nearly_equal(settings.target_temp_c, ui_now, 0.05f)) {
                                // Inna zmiana zewn─Ötrzna - tylko je┼Ťli r├│┼╝ni si─Ö znacz─ůco
                                main_screen_set_target_c(settings.target_temp_c);
                                main_screen_show_notification("Ustawienia zmienione zdalnie", 2000);
                                printf("[SYNC] Temperatura zaktualizowana: %.1f┬░C\n", settings.target_temp_c);
                            }
                        }
                    }
                    
                    // Aktualizuj cache
                    g_last_backend_set_c = settings.target_temp_c;
                    strncpy(g_last_backend_source, settings.last_source, sizeof(g_last_backend_source) - 1);
                    g_last_backend_source[sizeof(g_last_backend_source) - 1] = '\0';
                    g_have_backend_cache = true;
                    
                    main_screen_show_status("Po┼é─ůczono z serwerem", false);
                } else {
                    main_screen_show_status("B┼é─ůd komunikacji z serwerem", true);
                    printf("[NET] GET /device/%d/settings failed: %d\n", HB_DEVICE_ID, (int)gst);
                }
                
                last_server_check = now;
                g_pending_get_request = false;
            }

            // Cykliczne wysy┼éanie odczyt├│w
            if (now - last_http_post > READING_SEND_INTERVAL_MS) {
                if (!isnan(g_pending_setpoint)) {
                    // Wy┼Ťlij pending setpoint
                    send_reading_now();
                    g_pending_setpoint = NAN;
                } else {
                    // Standardowy odczyt
                    struct bme280_data d;
                    if (bme280_read_data(&d) == 0) {
                        float t  = d.temperature;
                        float rh = d.humidity;
                        float p  = d.pressure / 100.0f;
                        float set_c = main_screen_get_target_c();
                        
                        hb_http_status_t pst = hb_http_post_reading(HB_HOST, (uint16_t)HB_PORT, HB_DEVICE_ID, 
                                                                   t, rh, p, set_c, CONNECTION_TIMEOUT_MS);
                        if (pst == HB_HTTP_OK) {
                            printf("[NET] Heartbeat: temp=%.1f┬░C, setpoint=%.1f┬░C\n", t, set_c);
                        } else {
                            printf("[NET] POST /device/%d/reading failed: %d\n", HB_DEVICE_ID, (int)pst);
                        }
                    }
                }
                last_http_post = now;
            }

            // Wy┼é─ůczenie ochrony lokalnej zmiany po czasie
            if (local_override_active && now >= local_override_until_ms) {
                printf("[SYNC] Koniec ochrony lokalnej zmiany\n");
                local_override_active = false;
                // NIE pobieraj automatycznie z serwera - lokalna zmiana ma priorytet
                // g_pending_get_request = true; // USUNI─śTE - nie nadpisuj lokalnej zmiany
            }
        } else if (wifi_ok) {
            // WiFi skonfigurowane ale brak IP
            if (now - last_heartbeat > HEARTBEAT_INTERVAL_MS) {
                main_screen_show_status("Brak po┼é─ůczenia z internetem", true);
                last_heartbeat = now;
            }
        }
#endif
        
        // Dodatkowe funkcje systemowe co 60 sekund
        static uint32_t last_system_check = 0;
        if (now - last_system_check > 60000) {
            log_system_status();
            retry_failed_operations();
            
            // Sprawd┼║ WiFi reconnect je┼Ťli nie ma po┼é─ůczenia
            if (!wifi_connected && !have_ip_up()) {
                printf("[WIFI] Pr├│ba automatycznego reconnect...\n");
                main_screen_show_status("┼ü─ůczenie z WiFi...", false);
                if (wifi_connect_and_log()) {
                    wifi_connected = true;
                    int rssi = cyw43_wifi_get_rssi(&cyw43_state, CYW43_ITF_STA);
                    main_screen_update_wifi_status(true, rssi);
                    printf("[WIFI] Automatyczny reconnect pomy┼Ťlny\n");
                    main_screen_show_status("Po┼é─ůczono z WiFi", false);
                } else {
                    printf("[WIFI] Automatyczny reconnect nieudany\n");
                    main_screen_show_status("Brak po┼é─ůczenia WiFi", true);
                }
            }
            
            last_system_check = now;
        }
    }
}
