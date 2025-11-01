# CYW43 Troubleshooting Guide - HeatBeat-Pico

## Problem: "cyw43 error: hdr mismatch"

### Opis problemu
Błąd "hdr mismatch" w module CYW43 wskazuje na problemy z komunikacją między procesorem RP2040 a chipem WiFi. Może to być spowodowane:
- Nieprawidłową synchronizacją magistrali SPI
- Problemami z zasilaniem modułu WiFi
- Zakłóceniami elektromagnetycznymi
- Błędami w firmware CYW43

### Zastosowane rozwiązania w kodzie

#### 1. Stabilizacja inicjalizacji
```c
// Wielokrotne próby inicjalizacji z resetem
int init_attempts = 0;
const int max_init_attempts = 3;

while (init_attempts < max_init_attempts) {
    int init_result = cyw43_arch_init_with_country(CYW43_COUNTRY_POLAND);
    if (init_result == 0) break;
    
    // Soft reset przed kolejną próbą
    cyw43_arch_deinit();
    sleep_ms(1000);
}
```

#### 2. Dodatkowe opóźnienia stabilizacyjne
```c
// Po inicjalizacji
sleep_ms(500);

// Po włączeniu trybu STA
cyw43_arch_enable_sta_mode();
sleep_ms(200);

// Między próbami autoryzacji
sleep_ms(1000);
```

#### 3. Monitoring zdrowia CYW43
```c
static bool check_cyw43_health(void) {
    int link_status = cyw43_wifi_link_status(&cyw43_state, CYW43_ITF_STA);
    
    // Sprawdź poprawność statusu
    if (link_status < 0 || link_status > CYW43_LINK_BADAUTH) {
        return false;
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