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

#ifndef ENABLE_HTTP_CLIENT
#ifdef TEMP_DISABLE_WIFI
#if TEMP_DISABLE_WIFI
#define ENABLE_HTTP_CLIENT 0
#else
#define ENABLE_HTTP_CLIENT 1
#endif
#else
#define ENABLE_HTTP_CLIENT 1
#endif
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
#define HB_HOST "192.168.55.252"
#endif
#ifndef HB_PORT
#define HB_PORT 8000
#endif
#ifndef HB_DEVICE_ID
#define HB_DEVICE_ID 1
#endif

/* Jeśli masz endpoint ustawień (POST/PUT /device/{id}/settings),
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
    printf("[WiFi] proba polaczenia (auth=0x%08lx) do \"%s\"...\n", (unsigned long)auth, WIFI_SSID);
    
    // Dodatkowa stabilizacja przed próbą połączenia
    sleep_ms(100);
    
    int rc = cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASS, auth, 25000); // Oryginalny timeout jak w working version
    if (rc) { 
        printf("[WiFi] NIE polaczono (rc=%d)\n", rc); 
        
        // Sprawdź czy to błąd CYW43
        if (rc == PICO_ERROR_GENERIC || rc == PICO_ERROR_TIMEOUT) {
            printf("[WiFi] Możliwy błąd komunikacji z CYW43\n");
            sleep_ms(500); // Dodatkowe opóźnienie przy błędach komunikacji
        }
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
    
    int init_result = cyw43_arch_init_with_country(CYW43_COUNTRY_POLAND);
    if (init_result != 0) {
        printf("[CYW43] Błąd inicjalizacji: %d\n", init_result);
        return false;
    }
    
    printf("[CYW43] Inicjalizacja pomyślna\n");
    
    // Stabilizacja po inicjalizacji
    sleep_ms(500);
    
    cyw43_arch_enable_sta_mode();
    
    // Dodatkowa stabilizacja po włączeniu trybu STA
    sleep_ms(200);
    
    const uint32_t try_auths[] = { CYW43_AUTH_WPA2_AES_PSK, CYW43_AUTH_WPA2_MIXED_PSK, CYW43_AUTH_WPA_TKIP_PSK, CYW43_AUTH_OPEN };
    for (size_t i = 0; i < sizeof(try_auths)/sizeof(try_auths[0]); ++i) { 
        if (try_wifi_auth(try_auths[i]) == 0) return true; 
        sleep_ms(1000); // Zwiększone opóźnienie między próbami
    }
    printf("[WiFi] Nie udało się połączyć z \"%s\" - sprawdź hasło/SSID.\n", WIFI_SSID);
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

// Funkcja do obsługi błędów CYW43 i recovery
static bool cyw43_error_recovery(void) {
#if ENABLE_WIFI
    printf("[CYW43] Wykryto blad - proba recovery...\n");
    
    // Spróbuj miękki reset
    cyw43_arch_deinit();
    sleep_ms(2000); // Dłuższa pauza dla stabilności
    
    // Ponowna inicjalizacja
    if (cyw43_arch_init_with_country(CYW43_COUNTRY_POLAND) == 0) {
        printf("[CYW43] Recovery pomyślne - próba ponownego połączenia\n");
        cyw43_arch_enable_sta_mode();
        sleep_ms(500);
        
        // Spróbuj ponownie połączyć się z WiFi
        const uint32_t try_auths[] = { CYW43_AUTH_WPA2_AES_PSK, CYW43_AUTH_WPA2_MIXED_PSK, CYW43_AUTH_WPA_TKIP_PSK };
        for (size_t i = 0; i < sizeof(try_auths)/sizeof(try_auths[0]); ++i) { 
            if (try_wifi_auth(try_auths[i]) == 0) {
                printf("[CYW43] Recovery + reconnect pomyślne\n");
                return true;
            }
            sleep_ms(1000);
        }
        
        printf("[CYW43] Recovery udane, ale nie udało się połączyć z WiFi\n");
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
    // Sprawdź podstawowe funkcje CYW43
    int link_status = cyw43_wifi_link_status(&cyw43_state, CYW43_ITF_STA);
    
    // Sprawdź czy status jest w dozwolonym zakresie
    // Status 0 (DOWN) jest OK gdy nie ma połączenia
    if (link_status < CYW43_LINK_DOWN || link_status > CYW43_LINK_BADAUTH) {
        printf("[CYW43] Nieprawidłowy status: %d\n", link_status);
        return false;
    }
    
    // Jeśli mamy połączenie IP, ale link status to DOWN - problem
    if (have_ip_up() && link_status == CYW43_LINK_DOWN) {
        printf("[CYW43] Niespójność: mamy IP ale link DOWN\n");
        return false;
    }
    
    // Jeśli brak IP przez długi czas i status pokazuje link up - może być problem
    static int link_up_without_ip_count = 0;
    if (link_status == CYW43_LINK_UP && !have_ip_up()) {
        link_up_without_ip_count++;
        if (link_up_without_ip_count > 5) { // 5 sprawdzeń = 2.5 minuty
            printf("[CYW43] Link UP ale brak IP przez długi czas\n");
            link_up_without_ip_count = 0;
            return false;
        }
    } else {
        link_up_without_ip_count = 0;
    }
    
    // Test komunikacji przez RSSI tylko gdy jest połączenie
    static int consecutive_errors = 0;
    if (have_ip_up()) {
        int rssi = cyw43_wifi_get_rssi(&cyw43_state, CYW43_ITF_STA);
        if (rssi == 0) {
            // RSSI = 0 przy aktywnym połączeniu może wskazywać na problem
            consecutive_errors++;
            if (consecutive_errors > 3) {
                printf("[CYW43] Wielokrotne błędy RSSI, możliwy problem komunikacji\n");
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

// Funkcja do wykrywania błędów hdr mismatch
static void cyw43_monitor_errors(void) {
#if ENABLE_WIFI
    // Tutaj można dodać monitoring specific errors
    // Na razie bazowy monitoring w check_cyw43_health
#endif
}

// ========================= HTTP/Sync =========================
#if ENABLE_WIFI && ENABLE_HTTP_CLIENT
static uint32_t last_http_post = 0;
static bool     http_target_logged = false;
#endif

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

#if ENABLE_WIFI
// Status połączenia
static bool wifi_connected = false;
static int  last_rssi = 0;
static uint32_t last_recovery_attempt = 0;
static int recovery_attempts_count = 0;
#endif

// Parametry konfiguracyjne
#define SERVER_CHECK_INTERVAL_MS    5000   // Sprawdzanie serwera co 5s (dla testów)
#define READING_SEND_INTERVAL_MS    15000  // Wysyłanie odczytów co 15s
#define LOCAL_OVERRIDE_WINDOW_MS    5000   // Okno ochrony lokalnej zmiany
#define CONNECTION_TIMEOUT_MS       4000   // Timeout dla połączeń HTTP
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
        main_screen_show_status("Brak polaczenia", true);
        printf("[STARTUP] Brak polaczenia IP - pomijam sync\n");
        return;
    }
    
    // Nie nadpisuj aktywnych lokalnych zmian
    uint32_t now = to_ms_since_boot(get_absolute_time());
    if (local_override_active && now < local_override_until_ms) {
        printf("[STARTUP] Pomijam sync - aktywna lokalna zmiana (do %ums)\n", 
               (unsigned)local_override_until_ms);
        main_screen_show_status("Zachowuję lokalne ustawienia", false);
        return;
    }
    
    printf("[STARTUP] Rozpoczynam synchronizację z serwerem...\n");
            main_screen_show_status("Laczenie...", false);
    print_free_ram("Przed HTTP");
    
    // Dodatkowe opóźnienie dla stabilności połączenia
    sleep_ms(1000);
    
    printf("[STARTUP] Wysyłam żądanie GET do %s:%d\n", HB_HOST, HB_PORT);
    
    hb_settings_response_t settings;
    hb_http_status_t st = hb_http_get_settings(HB_HOST, (uint16_t)HB_PORT, HB_DEVICE_ID, 
                                              &settings, CONNECTION_TIMEOUT_MS);
    
    print_free_ram("Po HTTP");
        printf("[STARTUP] Odpowiedź z serwera: status=%d\n", (int)st);    if (st == HB_HTTP_OK) {
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
        
        // TYMCZASOWO WYŁĄCZONE - UI będzie aktualizowane później w main loop
        // printf("[STARTUP] Wywołuję main_screen_set_target_c...\n");
        // main_screen_set_target_c(settings.target_temp_c);
        // printf("[STARTUP] main_screen_set_target_c zakończone\n");
        
        printf("[STARTUP] Wywołuję main_screen_show_status...\n");
        main_screen_show_status("Połączono", false);
        printf("[STARTUP] main_screen_show_status zakończono\n");
        
        printf("[STARTUP] Pobrano ustawienia: temp=%.1f°C, source='%s'\n", 
               settings.target_temp_c, settings.last_source);
    } else {
        main_screen_show_status("Błąd komunikacji - pracuję offline", true);
        printf("[STARTUP] Błąd pobierania ustawień: %d\n", (int)st);
    }
    
    printf("[STARTUP] Synchronizacja zakończona\n");
    printf("[STARTUP] Oczekiwanie 5 sekund przed kontynuacją...\n");
    sleep_ms(5000);
    printf("[STARTUP] Kontynuacja po opóźnieniu\n");
}

// Natychmiastowe wysłanie odczytu z pending setpoint
static void send_reading_now(void) {
#if ENABLE_WIFI
    if (!have_ip_up()) {
        main_screen_show_status("Brak połączenia z serwerem", true);
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
        main_screen_show_notification("Wysłano do aplikacji", 2000);
        main_screen_set_notification_time(to_ms_since_boot(get_absolute_time()) + 2000);
        printf("[NET] POST reading: temp=%.1f°C, setpoint=%.1f°C\n", t, set_c);
    } else {
        main_screen_show_status("Nie udało się wysłać danych", true);
        printf("[NET] POST reading failed: %d\n", (int)pst);
    }
#endif
}

// Próba bezpośredniego ustawienia temperatury przez PUT
static bool try_direct_set_temperature(float new_target) {
    if (!have_ip_up()) return false;
    
    hb_http_status_t st = hb_http_set_settings_target_temp(HB_HOST, (uint16_t)HB_PORT, 
                                                          HB_DEVICE_ID, new_target, 
                                                          CONNECTION_TIMEOUT_MS);
    if (st == HB_HTTP_OK) {
        main_screen_show_notification("Temperatura\nzaktualizowana", 2000);
        main_screen_set_notification_time(to_ms_since_boot(get_absolute_time()) + 2000);
        printf("[NET] PUT settings: target=%.1f°C\n", new_target);
        return true;
    } else {
        printf("[NET] PUT settings failed: %d\n", (int)st);
        return false;
    }
}

// wywoływane z UI (slider/suwak/arc)
void heatbeat_on_target_temp_changed(float new_target) {
    uint32_t now = to_ms_since_boot(get_absolute_time());
    
    // 1) Aktywuj ochronę lokalnej zmiany
    local_override_active = true;
    local_override_value  = new_target;
    local_override_until_ms = now + LOCAL_OVERRIDE_WINDOW_MS;
    local_override_start_ms = now;
    
    printf("[LOCAL] Zmiana temperatury: %.1f°C (ochrona do %ums)\n", 
           new_target, (unsigned)local_override_until_ms);

    // 2) Spróbuj bezpośredniego ustawienia (PUT endpoint)
    bool put_success = try_direct_set_temperature(new_target);
    
    // 3) Jeśli PUT się nie udał, zapisz jako pending i wyślij przez reading
    if (!put_success) {
        g_pending_setpoint = new_target;
        main_screen_show_notification("Wysylanie...", 1500);
        main_screen_set_notification_time(now + 1500);
        
        // Natychmiastowa próba przez POST reading
        send_reading_now();
    }
    
    // 4) Zaplanuj szybkie sprawdzenie serwera za 2s
    g_pending_get_request = true;
}

// Funkcje dodatkowe do obsługi błędów i logowania
static void log_system_status(void) {
#if ENABLE_WIFI
    printf("[STATUS] WiFi: %s, Backend cache: %s, Local override: %s\n", 
           wifi_connected ? "Connected" : "Disconnected",
           g_have_backend_cache ? "Yes" : "No",
           local_override_active ? "Active" : "Inactive");
#else
    printf("[STATUS] WiFi: Disabled, Backend cache: %s, Local override: %s\n", 
           g_have_backend_cache ? "Yes" : "No",
           local_override_active ? "Active" : "Inactive");
#endif
    
    if (g_have_backend_cache) {
        printf("[STATUS] Backend: temp=%.1f°C, source='%s'\n", 
               g_last_backend_set_c, g_last_backend_source);
    }
}

// Funkcja do retry operacji
static void retry_failed_operations(void) {
    if (!isnan(g_pending_setpoint) && have_ip_up()) {
        printf("[RETRY] Ponowna próba wysłania pending setpoint: %.1f°C\n", g_pending_setpoint);
        send_reading_now();
    }
}

// ========================= MAIN =========================
int main(void) {
    gpio_init(BOOT_DIAG_LED); gpio_set_dir(BOOT_DIAG_LED, GPIO_OUT);
    for (int i=0;i<3;i++){ gpio_put(BOOT_DIAG_LED,1); sleep_ms(80); gpio_put(BOOT_DIAG_LED,0); sleep_ms(80); }

    stdio_usb_init();
    absolute_time_t t_limit = make_timeout_time_ms(3000);
    while (!stdio_usb_connected() && absolute_time_diff_us(get_absolute_time(), t_limit) > 0) sleep_ms(50);

    printf("\r\n=== HeatBeat-Pico start ===\r\n");
    print_free_ram("Boot");

#if ENABLE_WIFI
    bool wifi_ok = false;
#endif

#if ENABLE_WIFI
    {
        absolute_time_t wifi_deadline = make_timeout_time_ms(20000); // Zwiększony timeout
        printf("[BOOT] Start Wi-Fi init...\n");
        main_screen_show_status("Łączenie z WiFi...", false);
        
        int wifi_attempts = 0;
        const int max_wifi_attempts = 2;
        
        while (absolute_time_diff_us(get_absolute_time(), wifi_deadline) > 0 && wifi_attempts < max_wifi_attempts) { 
            wifi_attempts++;
            printf("[WIFI] Próba połączenia %d/%d\n", wifi_attempts, max_wifi_attempts);
            
            wifi_ok = wifi_connect_and_log(); 
            if (wifi_ok) {
                wifi_connected = true;
                int rssi = cyw43_wifi_get_rssi(&cyw43_state, CYW43_ITF_STA);
                main_screen_update_wifi_status(true, rssi);
                printf("[BOOT] WiFi połączony pomyślnie na próbie %d\n", wifi_attempts);
                break; 
            } else {
                printf("[BOOT] Próba WiFi %d nieudana\n", wifi_attempts);
                if (wifi_attempts < max_wifi_attempts) {
                    printf("[BOOT] Oczekiwanie przed kolejną próbą...\n");
                    printf("[BOOT] Deinicjalizacja CYW43 przed kolejną próbą...\n");
                    cyw43_arch_deinit();  // Wyczyść stan CYW43 przed kolejną próbą
                    sleep_ms(2000);
                }
            }
        }
        
        if (!wifi_ok) {
            printf("[BOOT] Wi-Fi SAFE-MODE: UI rusza bez sieci po %d próbach\n", wifi_attempts);
            main_screen_show_status("WiFi niedostępne - tryb offline", true);
            main_screen_update_wifi_status(false, 0);
        } else {
            // Startup sync z serwerem - z dodatkowym opóźnieniem dla stabilności i timeout
            printf("[BOOT] Przygotowanie do sync z serwerem...\n");
            sleep_ms(1000);
            
            // Uruchom sync w tle z timeout
            absolute_time_t sync_deadline = make_timeout_time_ms(10000); // 10s timeout
            printf("[BOOT] Rozpoczynam startup sync (timeout 10s)...\n");
            
            startup_sync_with_server();
            
            // Sprawdź czy sync się zakończył w czasie
            if (absolute_time_diff_us(get_absolute_time(), sync_deadline) <= 0) {
                printf("[BOOT] Startup sync - timeout!\n");
                main_screen_show_status("Timeout serwera - pracuję offline", true);
            }
            
            printf("[BOOT] Startup sync zakończony\n");
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
#if ENABLE_WIFI && ENABLE_HTTP_CLIENT
    uint32_t last_server_check = 0;  // Natychmiastowe pierwsze sprawdzenie
    uint32_t last_heartbeat = last_read;
#endif

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

        // Aktualizacja timerów UI
        main_screen_update_timers_with_time(now);
        
        // Sprawdzenie auto-lock ekranu - ważne dla blokady ekranu
        extern void check_auto_lock(void);
        check_auto_lock();

        // Aktualizacja UI z cache (tylko raz po starcie)
        static bool ui_cache_applied = false;
        if (!ui_cache_applied && g_have_backend_cache) {
            printf("[UI] Aktualizuję UI z cache: temp=%.1f°C\n", g_last_backend_set_c);
            main_screen_set_target_c(g_last_backend_set_c);
            ui_cache_applied = true;
        }

#if ENABLE_WIFI
        // Monitoring błędów CYW43
        cyw43_monitor_errors();
        // Monitoring zdrowia CYW43 co 30 sekund
        static uint32_t last_cyw43_check = 0;
        if (wifi_ok && (now - last_cyw43_check > 30000)) {
            if (!check_cyw43_health()) {
                // Unikaj zbyt częstych prób recovery (minimum 2 minuty między próbami)
                if (now - last_recovery_attempt > 120000) {
                printf("[CYW43] Wykryto problemy ze zdrowiem modułu\n");
                main_screen_show_status("Problem z modułem WiFi", true);                    last_recovery_attempt = now;
                    recovery_attempts_count++;
                    
                    // Maksymalnie 3 próby recovery na sesję
                    if (recovery_attempts_count <= 3) {
                        // Spróbuj recovery
                        if (cyw43_error_recovery()) {
                            main_screen_show_status("WiFi przywrócony", false);
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
                        main_screen_show_status("WiFi wyłączony - za dużo błędów", true);
                        printf("[CYW43] Wyłączam WiFi po %d nieudanych próbach recovery\n", recovery_attempts_count);
                        wifi_ok = false;
                        wifi_connected = false;
                        main_screen_update_wifi_status(false, 0);
                    }
                } else {
                    printf("[CYW43] Problemy wykryte, ale za wcześnie na kolejny recovery\n");
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

        // Komunikacja z serwerem
        if (wifi_connected && have_ip_up()) {
            // Sprawdzanie serwera - częściej przy pending request lub zgodnie z harmonogramem
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
                        printf("[NET] Nowe ustawienia: temp=%.1f°C, source='%s' (ui=%.1f°C)\n", 
                               settings.target_temp_c, settings.last_source, ui_now);
                        printf("[NET] Cache: temp=%.1f°C, source='%s', have_cache=%d\n",
                               g_last_backend_set_c, g_last_backend_source, g_have_backend_cache);
                    } else {
                        printf("[NET] Brak zmian: temp=%.1f°C, source='%s' (ui=%.1f°C)\n", 
                               settings.target_temp_c, settings.last_source, ui_now);
                    }

                    // Logika aktualizacji UI na podstawie source
                    if (local_override_active && now < local_override_until_ms) {
                        // W trakcie lokalnej zmiany - sprawdź czy serwer potwierdził
                        if (nearly_equal(settings.target_temp_c, local_override_value, 0.05f)) {
                            local_override_active = false;
                            main_screen_show_notification("Temperatura\npotwierdzona", 2000);
                            main_screen_set_notification_time(now + 2000);
                            printf("[SYNC] Serwer potwierdził %.1f°C\n", settings.target_temp_c);
                        } else {
                            printf("[SYNC] Ignoruję GET podczas lokalnej zmiany (serwer=%.1f, local=%.1f)\n",
                                   settings.target_temp_c, local_override_value);
                        }
                    } else {
                        // Sprawdź czy to zmiana z aplikacji - ma pierwszeństwo ZAWSZE
                        printf("[NET] Sprawdzanie synchronizacji: source='%s', changed=%d, local_active=%d, until=%u, now=%u\n",
                               settings.last_source, settings_changed, local_override_active, 
                               (unsigned)local_override_until_ms, (unsigned)now);
                               
                        if (strcmp(settings.last_source, "app") == 0) {
                            // Sprawdź czy source zmienił się z 'device' na 'app' lub temperatura się różni
                            bool source_changed_to_app = g_have_backend_cache && 
                                                        strcmp(g_last_backend_source, "app") != 0;
                            bool temp_differs = !nearly_equal(settings.target_temp_c, ui_now, 0.05f);
                            
                            if (source_changed_to_app || temp_differs) {
                                printf("[SYNC] Zmiana z aplikacji wykryta - source_changed=%d, temp_differs=%d\n", 
                                       source_changed_to_app, temp_differs);
                                printf("[SYNC] Anuluj lokalną ochronę - zmiana z aplikacji: %.1f°C\n", settings.target_temp_c);
                                local_override_active = false;
                                local_override_until_ms = 0;
                                main_screen_set_target_c_from_server(settings.target_temp_c, "app");
                            } else {
                                printf("[SYNC] Zmiana z aplikacji już zastosowana\n");
                            }
                        } else if (settings_changed) {
                            // Inne zmiany tylko jeśli rzeczywiście się zmieniły
                            if (local_override_until_ms > 0 && now < local_override_until_ms + 30000) {
                                // Daj 30s po zakończeniu ochrony zanim zezwolisz na inne nadpisanie
                                printf("[SYNC] Czekam 30s po lokalnej zmianie przed nadpisaniem (serwer=%.1f, ui=%.1f)\n",
                                       settings.target_temp_c, ui_now);
                            } else if (!nearly_equal(settings.target_temp_c, ui_now, 0.05f)) {
                                // Inna zmiana zewnętrzna - tylko jeśli różni się znacząco
                                main_screen_set_target_c(settings.target_temp_c);
                                main_screen_show_notification("Ustawienia zmienione zdalnie", 2000);
                                printf("[SYNC] Temperatura zaktualizowana: %.1f°C\n", settings.target_temp_c);
                            }
                        }
                    }
                    
                    // Aktualizuj cache
                    g_last_backend_set_c = settings.target_temp_c;
                    strncpy(g_last_backend_source, settings.last_source, sizeof(g_last_backend_source) - 1);
                    g_last_backend_source[sizeof(g_last_backend_source) - 1] = '\0';
                    g_have_backend_cache = true;
                    
                    main_screen_show_status("Połączono", false);
                } else {
                    main_screen_show_status("Błąd komunikacji", true);
                    printf("[NET] GET /device/%d/settings failed: %d\n", HB_DEVICE_ID, (int)gst);
                }
                
                last_server_check = now;
                g_pending_get_request = false;
            }

            // Cykliczne wysyłanie odczytów
            if (now - last_http_post > READING_SEND_INTERVAL_MS) {
                if (!isnan(g_pending_setpoint)) {
                    // Wyślij pending setpoint
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
                            printf("[NET] HeatBeat: temp=%.1f°C, setpoint=%.1f°C\n", t, set_c);
                        } else {
                            printf("[NET] POST /device/%d/reading failed: %d\n", HB_DEVICE_ID, (int)pst);
                        }
                    }
                }
                last_http_post = now;
            }

            // Wyłączenie ochrony lokalnej zmiany po czasie
            if (local_override_active && now >= local_override_until_ms) {
                printf("[SYNC] Koniec ochrony lokalnej zmiany\n");
                local_override_active = false;
                // NIE pobieraj automatycznie z serwera - lokalna zmiana ma priorytet
                // g_pending_get_request = true; // USUNIĘTE - nie nadpisuj lokalnej zmiany
            }
        } else if (wifi_ok) {
            // WiFi skonfigurowane ale brak IP
            if (now - last_heartbeat > HEARTBEAT_INTERVAL_MS) {
                main_screen_show_status("Brak połączenia z internetem", true);
                last_heartbeat = now;
            }
        }
#endif
        
        // Dodatkowe funkcje systemowe co 60 sekund
        static uint32_t last_system_check = 0;
        if (now - last_system_check > 60000) {
            log_system_status();
            retry_failed_operations();
            
#if ENABLE_WIFI
            // Sprawdź WiFi reconnect jeśli nie ma połączenia
            if (!wifi_connected && !have_ip_up()) {
                printf("[WIFI] Próba automatycznego reconnect...\n");
                main_screen_show_status("Łączenie z WiFi...", false);
                if (wifi_connect_and_log()) {
                    wifi_connected = true;
                    int rssi = cyw43_wifi_get_rssi(&cyw43_state, CYW43_ITF_STA);
                    main_screen_update_wifi_status(true, rssi);
                    printf("[WIFI] Automatyczny reconnect pomyślny\n");
                    main_screen_show_status("Połączono z WiFi", false);
                } else {
                    printf("[WIFI] Automatyczny reconnect nieudany\n");
                    main_screen_show_status("Brak połączenia WiFi", true);
                }
            }
#endif
            
            last_system_check = now;
        }
    }
}
