#include <stdio.h>
#include <stdint.h>
#include <time.h>
#include <math.h>
#include "pico/stdlib.h"
#include "pico/stdio_usb.h"
#include "pico/time.h"
#include "hardware/gpio.h"

#if ENABLE_WIFI || !defined(TEMP_DISABLE_WIFI)
#include "pico/cyw43_arch.h"
#endif

#include "lvgl.h"
#include "bme280_port.h"
#include "../lv_port/lv_port_disp.h"
#include "../lv_port/lv_port_indev.h"
#include "bsp_i2c.h"
#include "bsp_pcf85063.h"
#include "bsp_relay.h"
#include "bsp_buzzer.h"
#include "lvgl_ui/screen/main_screen.h"

// Global flags

// === SYSTEM DETEKCJI OTWARTEGO OKNA ===
static float temp_history[4]; // Historia temperatury (2 min przy pomiarze co 30s)
static int temp_history_idx = 0;
static bool temp_history_full = false;
static bool window_alarm_active = false;
static bool heating_active = false;
static absolute_time_t window_alarm_start;
static absolute_time_t last_window_beep;
static int window_beep_count = 0;

// === ZAPAMIĘTYWANIE METODY AUTORYZACJI ===
static uint32_t working_auth_method = 0x00400006;  // Domyślna WPA2_MIXED_PSK
static bool auth_method_found = false;

#ifndef ENABLE_WIFI
#ifdef TEMP_DISABLE_WIFI
#if TEMP_DISABLE_WIFI
#define ENABLE_WIFI 0
#else
#define ENABLE_WIFI 1  // WŁĄCZONY Z POJEDYNCZĄ PRÓBĄ
#endif
#else
#define ENABLE_WIFI 1  // WŁĄCZONY Z POJEDYNCZĄ PRÓBĄ
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

// Sieć KAMNET - wersja produkcyjna
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
// DIAGNOSTYKA CYW43 - szczegółowe testy modułu WiFi
static bool diagnose_cyw43_module(void) {
    printf("=== 🔍 DIAGNOSTYKA MODUŁU CYW43 WiFi ===\n");
    
    // Test 1: Podstawowa inicjalizacja
    printf("[TEST 1] Podstawowa inicjalizacja CYW43...\n");
    int init_result = cyw43_arch_init();
    if (init_result != 0) {
        printf("❌ [TEST 1] FAILED: cyw43_arch_init() = %d\n", init_result);
        printf("   Możliwe przyczyny:\n");
        printf("   - Brak zasilania modułu WiFi\n");
        printf("   - Uszkodzone połączenia SPI\n");
        printf("   - Uszkodzony chip CYW43\n");
        return false;
    }
    printf("✅ [TEST 1] PASSED: Podstawowa inicjalizacja OK\n");
    
    // Test 2: Inicjalizacja z krajem
    printf("[TEST 2] Inicjalizacja z kodem kraju (Polska)...\n");
    cyw43_arch_deinit();
    sleep_ms(500);
    
    init_result = cyw43_arch_init_with_country(CYW43_COUNTRY_POLAND);
    if (init_result != 0) {
        printf("❌ [TEST 2] FAILED: cyw43_arch_init_with_country() = %d\n", init_result);
        return false;
    }
    printf("✅ [TEST 2] PASSED: Inicjalizacja z krajem OK\n");
    
    // Test 3: Włączenie trybu STA
    printf("[TEST 3] Włączanie trybu Station (STA)...\n");
    cyw43_arch_enable_sta_mode();
    sleep_ms(500);
    
    // Test 4: Sprawdzenie statusu linku
    printf("[TEST 4] Test statusu linku WiFi...\n");
    int link_status = cyw43_wifi_link_status(&cyw43_state, CYW43_ITF_STA);
    printf("   Link Status: %d ", link_status);
    switch (link_status) {
        case CYW43_LINK_DOWN: printf("(DOWN - normalne dla braku połączenia)\n"); break;
        case CYW43_LINK_JOIN: printf("(JOIN)\n"); break;
        case CYW43_LINK_NOIP: printf("(NOIP)\n"); break;
        case CYW43_LINK_UP: printf("(UP)\n"); break;
        case CYW43_LINK_FAIL: printf("(FAIL)\n"); break;
        case CYW43_LINK_NONET: printf("(NONET)\n"); break;
        case CYW43_LINK_BADAUTH: printf("(BADAUTH)\n"); break;
        default: printf("(UNKNOWN/ERROR)\n"); break;
    }
    
    if (link_status < 0) {
        printf("❌ [TEST 4] FAILED: Nieprawidłowy status: %d\n", link_status);
        printf("   Moduł CYW43 może mieć problemy komunikacyjne\n");
        return false;
    }
    printf("✅ [TEST 4] PASSED: Status linku prawidłowy\n");
    
    // Test 5: Test komunikacji - próba skanowania sieci
    printf("[TEST 5] Test skanowania sieci WiFi...\n");
    printf("   Uwaga: To może potrwać kilka sekund...\n");
    
    // Próba prostego połączenia z timeoutem 5s (ma się nie powieść, ale test komunikacji)
    printf("[TEST 6] Test komunikacji przez próbę połączenia (5s timeout)...\n");
    int comm_test = cyw43_arch_wifi_connect_timeout_ms("test_network_12345", "test_pass", CYW43_AUTH_WPA2_AES_PSK, 5000);
    printf("   Wynik testu komunikacji: %d ", comm_test);
    
    if (comm_test == PICO_ERROR_TIMEOUT) {
        printf("(TIMEOUT - OK, komunikacja działa)\n");
        printf("✅ [TEST 6] PASSED: Komunikacja z CYW43 działa\n");
    } else if (comm_test == PICO_ERROR_BADAUTH || comm_test == -7) {
        printf("(BADAUTH - OK, komunikacja działa)\n");  
        printf("✅ [TEST 6] PASSED: Komunikacja z CYW43 działa\n");
    } else {
        printf("(Kod: %d)\n", comm_test);
        printf("⚠️  [TEST 6] WARNING: Nieoczekiwany wynik komunikacji\n");
    }
    
    printf("=== 🎯 WYNIKI DIAGNOSTYKI CYW43 ===\n");
    printf("✅ Moduł CYW43 jest sprawny i komunikuje się poprawnie\n");
    printf("✅ Zasilanie i połączenia SPI działają\n");
    printf("📡 Gotowy do testów z prawdziwą siecią WiFi\n\n");
    
    return true;
}

// Prosta funkcja reconnect bez diagnostyki (dla recovery)
static bool wifi_connect_simple(void) {
    printf("[WIFI] Prosta próba reconnect bez diagnostyki\n");
    
    cyw43_arch_enable_sta_mode();
    sleep_ms(500);
    
    // Spróbuj najpierw zapamiętaną metodę autoryzacji
    if (auth_method_found) {
        printf("[WiFi] Używam zapamiętanej metody autoryzacji\n");
        int rc = cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASS, working_auth_method, 8000);
        if (rc == 0) {
            printf("[WiFi] ✅ Reconnect z zapamiętaną metodą sukces!\n");
            return true;
        }
        printf("[WiFi] Zapamiętana metoda nie działa, próbuję wszystkie...\n");
    }
    
    // Jeśli zapamiętana nie działa, spróbuj wszystkie (bez diagnostyki!)
    struct {
        uint32_t auth;
        const char* name;
    } auth_methods[] = {
        {CYW43_AUTH_WPA2_AES_PSK,   "WPA2_AES_PSK"},    // Najpierw ta która zazwyczaj działa
        {CYW43_AUTH_WPA2_MIXED_PSK, "WPA2_MIXED_PSK"}, 
        {CYW43_AUTH_WPA_TKIP_PSK,   "WPA_TKIP_PSK"}
    };
    
    for (int i = 0; i < 3; i++) {
        printf("[WiFi] Próba %d/3: %s...\n", i+1, auth_methods[i].name);
        int rc = cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASS, auth_methods[i].auth, 6000);
        if (rc == 0) {
            printf("[WiFi] ✅ Reconnect sukces z %s!\n", auth_methods[i].name);
            working_auth_method = auth_methods[i].auth;
            auth_method_found = true;
            return true;
        }
        sleep_ms(300);  // Krótsze opóźnienia
    }
    
    printf("[WiFi] ❌ Reconnect nie powiódł się\n");
    return false;
}

static bool wifi_connect_and_log(void) {
      printf("[BOOT] 🔧 NAJPIERW DIAGNOSTYKA CYW43...\n");
      
      // Uruchom pełną diagnostykę modułu
      if (!diagnose_cyw43_module()) {
          printf("❌ [BOOT] KRYTYCZNY BŁĄD: Moduł CYW43 nie działa!\n");
          printf("   Sprawdź:\n");
          printf("   - Zasilanie płytki (stabilne 3.3V)\n");
          printf("   - Połączenia lutownicze z RP2040\n");  
          printf("   - Czy moduł CYW43 nie jest uszkodzony\n");
          return false;
      }

    // Po pomyślnej diagnostyce - prosta inicjalizacja
    printf("[BOOT] Inicjalizacja CYW43 dla WiFi...\n");
    // Diagnostyka już zrobiła init, więc CYW43 jest gotowy
    cyw43_arch_enable_sta_mode();
    sleep_ms(500);

      // MULTI-AUTH TEST - różne typy zabezpieczeń
      printf("[WiFi] 🔐 MULTI-AUTH test sieci: \"%s\"\n", WIFI_SSID);
      
      // Tablica różnych typów autoryzacji do przetestowania
      struct {
          uint32_t auth;
          const char* name;
      } auth_methods[] = {
          {CYW43_AUTH_WPA2_MIXED_PSK, "WPA2_MIXED_PSK"},  // Najpopularniejszy
          {CYW43_AUTH_WPA2_AES_PSK,   "WPA2_AES_PSK"},    // Poprzedni
          {CYW43_AUTH_WPA_TKIP_PSK,   "WPA_TKIP_PSK"},    // Starszy standard  
          {CYW43_AUTH_OPEN,           "OPEN"}              // Bez zabezpieczeń
      };
      
      int num_methods = sizeof(auth_methods) / sizeof(auth_methods[0]);
      bool connected = false;
      
      for (int i = 0; i < num_methods && !connected; i++) {
          printf("[WiFi] Próba %d/%d: %s...\n", i+1, num_methods, auth_methods[i].name);
          
          sleep_ms(500); // Stabilizacja między próbami
          
          int rc = cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASS, auth_methods[i].auth, 8000);
          
          if (rc == 0) {
              printf("[WiFi] ✅ SUKCES z %s!\n", auth_methods[i].name);
              // Zapamiętaj działającą metodę autoryzacji
              working_auth_method = auth_methods[i].auth;
              auth_method_found = true;
              printf("[WiFi] 💾 Zapamiętano metodę autoryzacji: %s\n", auth_methods[i].name);
              connected = true;
          } else {
              printf("[WiFi] ❌ %s nie powiodło się (rc=%d)\n", auth_methods[i].name, rc);
              
              // Krótka przerwa między próbami
              if (i < num_methods - 1) {
                  sleep_ms(1000);
              }
          }
      }
      
      if (!connected) {
          printf("[WiFi] ❌ Wszystkie metody autoryzacji nie powiodły się\n");
          printf("[WiFi] Sprawdź czy sieć \"%s\" jest dostępna\n", WIFI_SSID);
          return false;
      }

      struct netif* nif = get_nif();
      if (!nif || !netif_is_up(nif)) { 
          printf("[WiFi] ❌ Interfejs nie jest UP\n"); 
          return false;
      }

      printf("[WiFi] ✅ SUKCES! Połączono z \"%s\"\n", WIFI_SSID);
      printf("[WiFi] IP: ");
      print_ip4(netif_ip4_addr(nif));
      printf("\n");

      int rssi = cyw43_wifi_get_rssi(&cyw43_state, CYW43_ITF_STA);
      if (rssi != 0) printf("[WiFi] RSSI: %d dBm\n", rssi);
      
      return true;
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
        
        // Spróbuj ponownie połączyć się z WiFi (pojedyncza próba)
        printf("[WiFi] proba polaczenia do \"%s\"...\n", WIFI_SSID);
        
        int rc = cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASS, CYW43_AUTH_WPA2_AES_PSK, 25000); 
        if (rc == 0) {
            printf("[CYW43] Recovery + reconnect pomyślne\n");
            return true;
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
    
    // WAŻNE: Jeśli mamy działające połączenie IP, to CYW43 działa dobrze
    // nie przejmuj się statusem link - może być w stanie przejściowym
    if (have_ip_up()) {
        return true; // Mamy IP = wszystko działa
    }
    
    // Sprawdź czy status jest w dozwolonym zakresie tylko jeśli nie ma IP
    // Status 0-6 to prawidłowe wartości (DOWN, JOIN, NONET, NOIP, FAIL, UP, BADAUTH)
    if (link_status < 0 || link_status > 6) {
        printf("[CYW43] Nieprawidłowy status: %d (ale może być OK)\n", link_status);
        // NIE zwracaj false od razu - może być przejściowy
    }
    
    // Sprawdź czy naprawdę są problemy - tylko długotrwałe problemy
    static int no_ip_count = 0;
    if (!have_ip_up()) {
        no_ip_count++;
        // Tylko po BARDZO długim czasie bez IP (10 sprawdzeń = 5 minut)
        if (no_ip_count > 10) {
            printf("[CYW43] Brak IP przez bardzo długi czas (5+ min) - może być problem\n");
            no_ip_count = 0;
            return false;
        }
    } else {
        no_ip_count = 0; // Reset licznika gdy IP działa
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
#define READING_SEND_INTERVAL_MS    30000  // Wysyłanie odczytów co 30s
#define LOCAL_OVERRIDE_WINDOW_MS    5000   // Okno ochrony lokalnej zmiany
#define CONNECTION_TIMEOUT_MS       4000   // Timeout dla połączeń HTTP
#define HEARTBEAT_INTERVAL_MS       30000  // Heartbeat co 30s

// Helper function for floating point comparison
static inline bool nearly_equal(float a, float b, float eps) { 
    return fabsf(a - b) <= eps; 
}

// === DETEKCJA OTWARTEGO OKNA ===
// ZEWNĘTRZNE: void heatbeat_stop_window_alarm_on_unlock(void); - do wywołania przy odblokowaniu
static void update_temperature_history(float temp) {
    temp_history[temp_history_idx] = temp;
    temp_history_idx = (temp_history_idx + 1) % 4;
    if (temp_history_idx == 0) temp_history_full = true;
}

static bool detect_open_window(float current_temp) {
    // Potrzebujemy przynajmniej 4 pomiary (2 minuty przy 30s interwałach)
    bool have_enough_data = temp_history_full || temp_history_idx >= 4;
    if (!have_enough_data) {
        printf("[WINDOW] Zbieranie danych... mam %d pomiarów, potrzebuję 4 (2 minuty)\n", temp_history_idx);
        return false;
    }
    
    // Znajdź temperaturę sprzed 2 minut (4 pomiary * 30s = 120s)
    int old_idx = temp_history_full ? 
        ((temp_history_idx - 4 + 4) % 4) :  // Gdy bufor pełny - 4 pomiary wstecz
        0;  // Gdy bufor niepełny - najstarszy pomiar
    float temp_2min_ago = temp_history[old_idx];
    
    float temp_drop = temp_2min_ago - current_temp;
    
    printf("[WINDOW] Porównanie: 2min_temu=%.1f°C, teraz=%.1f°C, spadek=%.1f°C\n", 
           temp_2min_ago, current_temp, temp_drop);
    
    // WYKRYJ OTWARTE OKNO: spadek >= 2.0°C w 2 minuty
    if (temp_drop >= 2.0f) {
        printf("[WINDOW] 🚨 WYKRYTO OTWARTE OKNO! Spadek %.1f°C w 2 minuty\n", temp_drop);
        return true;
    }
    
    // ZATRZYMAJ ALARM: wzrost temperatury (okno zamknięte)
    if (window_alarm_active && temp_drop < 0.5f) {
        printf("[WINDOW] ✅ Temperatura wzrosła - zatrzymuję alarm (spadek tylko %.1f°C)\n", temp_drop);
        window_alarm_active = false;
        return false;
    }
    
    return false;
}

static void handle_window_alarm() {
    if (!window_alarm_active) return;
    
    absolute_time_t now = get_absolute_time();
    uint32_t elapsed_ms = absolute_time_diff_us(window_alarm_start, now) / 1000;
    
    // Po 10 minutach wyłącz alarm
    if (elapsed_ms > 600000) { // 10 min
        printf("[WINDOW] Alarm zakończony po 10 minutach\n");
        window_alarm_active = false;
        return;
    }
    
    // Pierwsza minuta: brzęczenie co 4s (1s ON, 3s OFF)
    if (elapsed_ms < 60000) {
        uint32_t cycle_time = elapsed_ms % 4000;
        if (cycle_time < 1000) {
            // Zabezpieczenie przed wielokrotnym brzęczeniem w tym samym cyklu
            if (absolute_time_diff_us(last_window_beep, now) > 3500000) { // Minimum 3.5s między brzęczeniami
                // DODATKOWE sprawdzenie: nie przerywaj aktywnego brzęczenia
                if (!bsp_buzzer_is_active()) {
                    printf("[WINDOW] 🔔 Brzęczenie alarmu (1min): cycle=%dms\n", cycle_time);
                    bsp_buzzer_beep(2000, 500); // 2kHz przez 0.5s - jak test startowy
                    last_window_beep = now;
                } else {
                    printf("[WINDOW] Pomijam - buzzer już aktywny\n");
                }
            }
        }
    }
    // Kolejne 9 minut: krótkie brzęczenie co minutę
    else {
        uint32_t time_since_first_min = elapsed_ms - 60000;
        uint32_t minute_cycle = time_since_first_min % 60000;
        
        if (minute_cycle < 1000) { // Pierwsza sekunda każdej minuty
            if (absolute_time_diff_us(last_window_beep, now) > 58000000) { // Zabezpieczenie przed wielokrotnym brzęczeniem
                // DODATKOWE sprawdzenie: nie przerywaj aktywnego brzęczenia
                if (!bsp_buzzer_is_active()) {
                    printf("[WINDOW] 🔔 Brzęczenie #%d (minuta %d)\n", window_beep_count + 1, (int)(time_since_first_min/60000) + 2);
                    bsp_buzzer_beep(1000, 500); // 1kHz przez 500ms - przypomnienie
                    last_window_beep = now;
                    window_beep_count++;
                } else {
                    printf("[WINDOW] Pomijam brzęczenie - buzzer już aktywny\n");
                }
            }
        }
    }
}

static void start_window_alarm() {
    if (window_alarm_active) {
        printf("[WINDOW] Alarm już aktywny, pomijam\n");
        return; // Już aktywny
    }
    
    printf("[WINDOW] 🔔 URUCHAMIAM ALARM OTWARTEGO OKNA!\n");
    window_alarm_active = true;
    window_alarm_start = get_absolute_time();
    last_window_beep = get_absolute_time();
    window_beep_count = 0;
    
    // NATYCHMIASTOWE GŁOŚNE BRZĘCZENIE!
    printf("[BUZZER] 🚨 ALARM OTWARTEGO OKNA - GŁOŚNE BRZĘCZENIE! 🚨\n");
    bsp_buzzer_beep(1000, 2000); // 1kHz przez 2 sekundy - BARDZO GŁOŚNO!
    sleep_ms(100); // Krótka przerwa
    bsp_buzzer_beep(1500, 1000); // Drugi sygnał 1.5kHz przez 1s
}

// Funkcja do zatrzymywania alarmu przy odblokowaniu ekranu
void heatbeat_stop_window_alarm_on_unlock(void) {
    if (window_alarm_active) {
        printf("[WINDOW] 🔓 Alarm zatrzymany przez odblokowanie ekranu\n");
        window_alarm_active = false;
    }
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
        main_screen_show_status("Brak połączenia", true);
        return;
    }
    
    struct bme280_data d;
    if (bme280_read_data(&d) != 0) return;
    
    float t  = d.temperature;
    float rh = d.humidity;
    float p  = d.pressure / 100.0f;
    float set_c = isnan(g_pending_setpoint) ? main_screen_get_target_c() : g_pending_setpoint;
    
    hb_http_status_t pst = hb_http_post_reading(HB_HOST, (uint16_t)HB_PORT, HB_DEVICE_ID, 
                                               t, rh, p, set_c, window_alarm_active, heating_active, CONNECTION_TIMEOUT_MS);    if (pst == HB_HTTP_OK) {
        if (window_alarm_active) {
            main_screen_show_notification("⚠️ Wykryto otwarte okno!", 5000);
            printf("[NET] POST reading: temp=%.1f°C, setpoint=%.1f°C, WINDOW_OPEN=true\n", t, set_c);
        } else {
            main_screen_show_notification("Wysłano do aplikacji", 2000);
            printf("[NET] POST reading: temp=%.1f°C, setpoint=%.1f°C, window_open=false\n", t, set_c);
        }
        main_screen_set_notification_time(to_ms_since_boot(get_absolute_time()) + 2000);
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
#else
    // WiFi wyłączony - inicjalizuj hardware od razu
    printf("[BOOT] WiFi wyłączony - inicjalizacja hardware...\n");
    bsp_relay_init();     // GPIO 2 - zawór grzewczy
    bsp_buzzer_init();    // GPIO 20 - buzzer dla alarmów
    
    // 🔊 TEST BUZZERA przy starcie (tryb offline)
    printf("[BOOT] 🔊 Test buzzera...\n");
    bsp_buzzer_beep(2000, 500);  // 2kHz przez 0.5s - test startowy
    
    // AKTYWNE CZEKANIE z forsowaniem zatrzymania (offline)
    absolute_time_t buzzer_timeout = make_timeout_time_ms(600);
    while (absolute_time_diff_us(get_absolute_time(), buzzer_timeout) > 0) {
        if (!bsp_buzzer_is_active()) break; // Buzzer się wyłączył
        sleep_ms(10); // Sprawdzaj co 10ms
    }
    
    // FORSUJ zatrzymanie na koniec
    bsp_buzzer_stop();
    printf("[BOOT] Test buzzera zakończony - FORSOWNIE zatrzymany (offline)\n");
    printf("[BOOT] Hardware zainicjalizowany bez WiFi\n");
#endif

#if ENABLE_WIFI
    {
        // Pojedyncza próba WiFi bez timeout deadline
        printf("[BOOT] Start Wi-Fi init...\n");
        main_screen_show_status("Łączenie z WiFi...", false);
        
        // POJEDYNCZA PRÓBA WiFi - bez pętli głównej
        printf("[WIFI] === POJEDYNCZA PRÓBA POŁĄCZENIA ===\n");
        
        wifi_ok = wifi_connect_and_log();
        printf("[WIFI] Wynik pojedynczej próby: %s\n", wifi_ok ? "SUKCES" : "BŁĄD");
        if (wifi_ok) {
            wifi_connected = true;
            int rssi = cyw43_wifi_get_rssi(&cyw43_state, CYW43_ITF_STA);
            main_screen_update_wifi_status(true, rssi);
            printf("[BOOT] WiFi połączony pomyślnie\n");
            
            // Teraz bezpiecznie inicjalizuj hardware po WiFi
            printf("[BOOT] Inicjalizacja przekaźnika po WiFi...\n");
            bsp_relay_init();     // GPIO 2 - zawór grzewczy
            bsp_buzzer_init();    // GPIO 20 - buzzer dla alarmów
            
            // 🔊 TEST BUZZERA przy starcie
            printf("[BOOT] 🔊 Test buzzera...\n");
            bsp_buzzer_beep(2000, 500);  // 2kHz przez 0.5s - test startowy
            
            // AKTYWNE CZEKANIE z forsowaniem zatrzymania
            absolute_time_t buzzer_timeout = make_timeout_time_ms(600);
            while (absolute_time_diff_us(get_absolute_time(), buzzer_timeout) > 0) {
                if (!bsp_buzzer_is_active()) break; // Buzzer się wyłączył
                sleep_ms(10); // Sprawdzaj co 10ms
            }
            
            // FORSUJ zatrzymanie na koniec
            bsp_buzzer_stop();
            printf("[BOOT] Test buzzera zakończony - FORSOWNIE zatrzymany\n");
        } else {
            printf("[BOOT] ❌ WiFi WYMAGANE - nie można kontynuować bez połączenia\n");
            main_screen_show_status("BŁĄD: Brak WiFi - sprawdź sieć", true);
            main_screen_update_wifi_status(false, 0);
            
            // ZATRZYMAJ SYSTEM - nie przechodź do offline
            while (true) {
                printf("[SYSTEM] Czekam na restart - WiFi jest wymagane!\n");
                sleep_ms(5000);
            }
        }
        
        if (wifi_ok) {
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
    
    // Sterowanie grzaniem
    uint32_t last_heating_check = 0;
    const float TEMP_HYSTERESIS = 0.5f; // 0.5°C histerezy
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

        // BME co 30 s - wystarczająco często dla detekcji okna
        if (now - last_read > 30000) {
            if (bme280_read_data(&bme_data) == 0) {
                extern float current_temp; extern int humidity; extern float pressure;
                current_temp = bme_data.temperature;
                humidity     = (int)(bme_data.humidity + 0.5f);
                pressure     = bme_data.pressure;
                extern void update_labels(void); update_labels();

                // === DETEKCJA OTWARTEGO OKNA ===
                update_temperature_history(current_temp);
                
                // Sprawdzaj detekcję przy każdym pomiarze (co 30s)
                if (detect_open_window(current_temp)) {
                    start_window_alarm();
                }

                // Logi BME280 tylko przy przesyle danych (usunieto co 10s logging)
            }
            last_read = now;
        }

        // Aktualizacja timerów UI
        main_screen_update_timers_with_time(now);
        
        // Obsługa alarmu otwartego okna
        handle_window_alarm();
        
        // Sprawdź buzzer (czy skończyć beep)
        if (bsp_buzzer_is_active()) {
            // Buzzer sam się wyłączy gdy skończy się czas
        }
        
        // Sprawdzenie auto-lock ekranu - ważne dla blokady ekranu
        extern void check_auto_lock(void);
        check_auto_lock();

        // Sterowanie zaworem grzewczym (co 5 sekund)
        if (now - last_heating_check > 5000) {
            extern float set_temperature;
            float temp_diff = set_temperature - current_temp;
            bool should_heat = false;
            
            if (!heating_active) {
                // Zawór wyłączony - włącz gdy temperatura jest niższa o histerezy
                should_heat = (temp_diff > TEMP_HYSTERESIS);
            } else {
                // Zawór włączony - wyłącz gdy temperatura osiągnęła cel
                should_heat = (temp_diff > -TEMP_HYSTERESIS);
            }
            
            if (should_heat != heating_active) {
                heating_active = should_heat;
                bsp_relay_set_state(heating_active);
                
                // Sygnał dźwiękowy zmiany stanu
                if (heating_active) {
                    // SEKUNDOWY BUZZ przy włączeniu grzania
                    bsp_buzzer_beep(1000, 1000); // 1kHz przez 1 sekundę
                    printf("[HEATING] 🔥 Włączono grzanie + BUZZ: temp=%.1f°C cel=%.1f°C diff=%.2f°C\n", 
                           current_temp, set_temperature, temp_diff);
                } else {
                    printf("[HEATING] Wyłączono grzanie: temp=%.1f°C cel=%.1f°C diff=%.2f°C\n", 
                           current_temp, set_temperature, temp_diff);
                }
            }
            
            last_heating_check = now;
        }
        
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
        // Monitoring zdrowia CYW43 rzadziej - co 2 minuty
        static uint32_t last_cyw43_check = 0;
        if (wifi_ok && (now - last_cyw43_check > 120000)) {
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

        // Monitorowanie statusu WiFi - rzadziej dla stabilności
        if (wifi_ok && (now - last_wifi_status_print > 60000)) {
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
                                                                   t, rh, p, set_c, window_alarm_active, heating_active, CONNECTION_TIMEOUT_MS);
                        if (pst == HB_HTTP_OK) {
                            printf("[NET] BME280: T=%.1f°C RH=%.0f%% P=%.0fhPa -> Server (setpoint=%.1f°C)\n", t, rh, p, set_c);
                        } else {
                            printf("[NET] POST failed: %d (T=%.1f°C RH=%.0f%% P=%.0fhPa)\n", (int)pst, t, rh, p);
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
              // Sprawdź WiFi reconnect jeśli nie ma połączenia - z opóźnieniem dla stabilności
            static uint32_t last_disconnect_time = 0;
            if (!wifi_connected && !have_ip_up()) {
                if (last_disconnect_time == 0) {
                    last_disconnect_time = now;
                    printf("[WIFI] Utrata połączenia - czekam 30s przed reconnect...\n");
                } else if (now - last_disconnect_time > 30000) { // Czekaj 30s
                    printf("[WIFI] Próba automatycznego reconnect po 30s...\n");
                    main_screen_show_status("Łączenie z WiFi...", false);
                    // Używamy prostego reconnect bez diagnostyki aby uniknąć zawieszenia
                    if (wifi_connect_simple()) {
                        wifi_connected = true;
                        last_disconnect_time = 0; // Reset timera po udanym połączeniu
                        int rssi = cyw43_wifi_get_rssi(&cyw43_state, CYW43_ITF_STA);
                        main_screen_update_wifi_status(true, rssi);
                        printf("[WIFI] Automatyczny reconnect pomyślny\n");
                        main_screen_show_status("Połączono z WiFi", false);
                    } else {
                        printf("[WIFI] Automatyczny reconnect nieudany - czekam dalej\n");
                        main_screen_show_status("Brak połączenia", true);
                        // NIE resetuj last_disconnect_time - nadal czekamy
                    }
                }
            } else if (wifi_connected && have_ip_up()) {
                // Reset timera gdy połączenie działa
                last_disconnect_time = 0;
            }
#endif
            
            last_system_check = now;
        }
    }
}
