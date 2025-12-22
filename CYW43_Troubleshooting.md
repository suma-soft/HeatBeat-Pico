# CYW43 Troubleshooting Guide - HeatBeat-Pico

## Problem: "cyw43 error: hdr mismatch"

### Opis problemu
Błąd "hdr mismatch" w module CYW43 wskazuje na problemy z komunikacją między procesorem RP2040 a chipem WiFi. Może to być spowodowane:
- Nieprawidłową synchronizacją magistrali SPI
- Problemami z zasilaniem modułu WiFi
- Zakłóceniami elektromagnetycznymi
- Błędami w firmware CYW43

### Zastosowane rozwiązania w kodzie

#### 1. Kompletna diagnostyka przy starcie
```c
// Funkcja diagnostyczna wywołana przed główną inicjalizacją
void test_cyw43_hardware() {
    printf("[TEST 1] Podstawowa inicjalizacja CYW43...\n");
    printf("[TEST 2] Inicjalizacja z kodem kraju (Polska)...\n"); 
    printf("[TEST 3] Włączanie trybu Station (STA)...\n");
    printf("[TEST 4] Test statusu linku WiFi...\n");
    printf("[TEST 5] Test skanowania sieci WiFi...\n");
    printf("[TEST 6] Test komunikacji przez próbę połączenia (5s timeout)...\n");
}

// Rezultaty testów z polskimi komunikatami
printf("[TEST 1] SUKCES: Podstawowa inicjalizacja OK\n");
printf("[TEST 6] SUKCES: Komunikacja z CYW43 działa\n");
```

#### 2. Multi-Auth system z diagnostyką
```c
// Tablica różnych metod autoryzacji  
struct auth_method_t {
    cyw43_auth_t auth;
    const char* name;
};

struct auth_method_t auth_methods[] = {
    {CYW43_AUTH_WPA2_AES_PSK, "WPA2-AES"},
    {CYW43_AUTH_WPA2_MIXED_PSK, "WPA2-Mixed"}, 
    {CYW43_AUTH_WPA2_AES_PSK, "WPA2-Fallback"}
};

// Próba każdej metody z logowaniem
printf("[WiFi] Próba %d/%d: %s...\n", i+1, num_methods, auth_methods[i].name);
printf("[WiFi] SUKCES z %s!\n", auth_methods[i].name);
```

#### 3. Recovery system z CYW43
```c
// Funkcja recovery przy błędach komunikacji
bool recover_cyw43_error() {
    printf("[CYW43] Wykryto błąd - próba recovery...\n");
    
    cyw43_arch_deinit();
    sleep_ms(2000);  // Dłuższe opóźnienie
    
    if (cyw43_arch_init_with_country(CYW43_COUNTRY_POLAND) == 0) {
        printf("[CYW43] Recovery pomyślne - próba ponownego połączenia\n");
        return true;
    }
    return false;
}
    }
    
    // Test komunikacji przez RSSI
    int rssi = cyw43_wifi_get_rssi(&cyw43_state, CYW43_ITF_STA);
    // Monitoring consecutive errors...
}
```

#### 4. Recovery mechanizm
```c
static bool cyw43_error_recovery(void) {
    printf("[CYW43] Wykryto błąd - próba recovery...\n");
    
    cyw43_arch_deinit();
    sleep_ms(2000); // Dłuższa pauza
    
    if (cyw43_arch_init_with_country(CYW43_COUNTRY_POLAND) == 0) {
        cyw43_arch_enable_sta_mode();
        sleep_ms(500);
        return true;
    }
    return false;
}
```

### Dodatkowe środki zaradcze

#### Sprzętowe
1. **Sprawdź zasilanie:**
   - Upewnij się, że zasilanie 3.3V jest stabilne
   - Użyj oscyloskopu do sprawdzenia tętnienia
   - Dodaj kondensatory filtrujące blisko modułu

2. **Ekranowanie:**
   - Umieść urządzenie z dala od źródeł zakłóceń
   - Sprawdź czy nie ma problemów z masą

3. **Temperatura:**
   - Sprawdź czy moduł nie przegrzewa się
   - Zapewnij odpowiednie chłodzenie

#### Programowe
1. **Redukcja częstotliwości SPI:**
   ```c
   // W niektórych przypadkach pomaga obniżenie prędkości komunikacji
   // (wymaga modyfikacji SDK)
   ```

2. **Zwiększ timeouty:**
   ```c
   #define WIFI_CONNECT_TIMEOUT_MS 25000  // Zwiększony timeout
   ```

3. **Monitoring częstszy:**
   ```c
   // Sprawdzaj stan CYW43 co 30 sekund zamiast rzadziej
   static uint32_t last_cyw43_check = 0;
   if (now - last_cyw43_check > 30000) {
       check_cyw43_health();
   }
   ```

### Logi diagnostyczne

#### Normalne działanie
```
[CYW43] Inicjalizacja pomyślna
[WiFi] Połączono z "NETWORK" IP:192.168.1.100
[WiFi] RSSI: -45 dBm
```

#### Wykryte problemy
```
[CYW43] Błąd inicjalizacji: -1
[CYW43] Próba soft reset...
[CYW43] Wykryto problemy ze zdrowiem modułu
[CYW43] Wielokrotne błędy RSSI, możliwy problem komunikacji
```

#### Recovery
```
[CYW43] Wykryto błąd - próba recovery...
[CYW43] Recovery pomyślne
[WiFi] Po recovery - próba ponownego połączenia
```

### Parametry konfiguracyjne

#### Timery stabilizacyjne
```c
#define CYW43_INIT_RETRY_COUNT      3       // Próby inicjalizacji
#define CYW43_RESET_DELAY_MS        1000    // Opóźnienie po reset
#define CYW43_STABILIZATION_MS      500     // Stabilizacja po init
#define CYW43_STA_DELAY_MS          200     // Opóźnienie po STA enable
#define CYW43_AUTH_RETRY_DELAY_MS   1000    // Między próbami auth
```

#### Monitoring
```c
#define CYW43_HEALTH_CHECK_INTERVAL_MS  30000   // Sprawdzanie zdrowia
#define CYW43_RSSI_ERROR_THRESHOLD      3       // Próg błędów RSSI
#define CYW43_RECOVERY_TIMEOUT_MS       2000    // Timeout recovery
```

### Alternatywne rozwiązania

#### 1. Hard reset przez GPIO
```c
// Jeśli dostępny pin RESET dla CYW43
void cyw43_hard_reset(void) {
    gpio_put(CYW43_RESET_PIN, 0);
    sleep_ms(100);
    gpio_put(CYW43_RESET_PIN, 1);
    sleep_ms(500);
}
```

#### 2. Watchdog dla CYW43
```c
// Automatyczny restart całego systemu w przypadku krytycznych błędów
void setup_cyw43_watchdog(void) {
    watchdog_enable(10000, 1); // 10s timeout
}
```

#### 3. Fallback mode
```c
// Kompletne wyłączenie WiFi w trybie awaryjnym
bool enable_fallback_mode = false;

if (cyw43_consecutive_failures > 5) {
    enable_fallback_mode = true;
    main_screen_show_status("Tryb awaryjny - WiFi wyłączone", true);
}
```

### Testowanie

#### Test stabilności
1. Uruchom urządzenie na 24h
2. Monitoruj logi błędów CYW43
3. Sprawdź czy recovery działa poprawnie
4. Zweryfikuj czy nie ma memory leaks

#### Test odporności
1. Symuluj zakłócenia elektromagnetyczne
2. Test przy różnych temperaturach
3. Test z różnymi źródłami zasilania
4. Test długotrwałej pracy

#### Test recovery
1. Wymuś błąd CYW43 (np. przez debugging)
2. Sprawdź czy recovery się uruchamia
3. Zweryfikuj powrót do normalnej pracy
4. Test wielokrotnych recovery

## Wniosek
Implementowane rozwiązania znacząco zwiększają stabilność połączenia WiFi poprzez:
- Wielokrotne próby inicjalizacji z soft reset
- Aktywny monitoring zdrowia modułu CYW43
- Automatyczny recovery przy błędach
- Graceful degradation do trybu offline
- Szczegółowe logowanie dla diagnostyki

Te mechanizmy powinny rozwiązać problem "hdr mismatch" i zwiększyć ogólną niezawodność systemu.