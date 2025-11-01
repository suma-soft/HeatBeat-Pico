# Testing Emergency Recovery System

## Instrukcja testowania systemu zabezpieczeń termostatu

### Przygotowanie do testów

1. **Kompilacja firmware z debug logami:**
   ```bash
   # W VS Code naciśnij Ctrl+Shift+P
   # Wybierz "Tasks: Run Task" > "Compile Project"
   ```

2. **Podłączenie termostatu:**
   - Podłącz Pico W przez USB
   - Otwórz monitor szeregowy (115200 baud)
   - Naciśnij reset na Pico

3. **Weryfikacja systemu:**
   ```
   🛡️ Watchdog włączony (60s timeout)
   [BOOT] Start Wi-Fi init...
   ```

## Test 1: Emergency Timeout przy starcie (Łagodny)

### Cel:
Przetestować timeout w `startup_sync_with_server()` przy niedostępnym serwerze.

### Kroki:
1. **Zmień adres serwera na nieistniejący:**
   ```c
   // W main.c zmień:
   #define HB_HOST "192.168.55.119"
   // Na:
   #define HB_HOST "192.168.55.999"  // Nieistniejący adres
   ```

2. **Kompilacja i upload:**
   ```bash
   # Kompiluj i wgraj firmware
   ```

3. **Obserwacja logów:**
   ```
   [STARTUP] ⏱️ Ustawiono emergency timeout na 30s
   [STARTUP] Wysyłam żądanie GET do 192.168.55.999:8000
   [HTTP] run_client: start (timeout=10000ms)
   ...
   [STARTUP] 🚨 EMERGENCY TIMEOUT po GET settings!
   [STARTUP] Synchronizacja zakończona
   ```

### Oczekiwany rezultat:
- Po ~30 sekundach: emergency timeout
- System kontynuuje pracę offline
- Brak watchdog reboot

## Test 2: HTTP Client Hang Simulation (Średni)

### Cel:
Przetestować emergency abort w warstwie HTTP client.

### Kroki:
1. **Włącz test mode:**
   ```c
   // W main.c odkomentuj:
   #define EMERGENCY_TEST_MODE
   
   // W hb_http.c odkomentuj:
   #define EMERGENCY_TEST_MODE
   ```

2. **Ustaw niski próg testu:**
   ```c
   // W main.c zmień:
   #define TEST_HANG_AFTER_COUNT 50000
   // Na:
   #define TEST_HANG_AFTER_COUNT 100  // Zawiś się szybko
   ```

3. **Skonfiguruj poprawny serwer:**
   ```c
   #define HB_HOST "192.168.55.119"  // Poprawny adres
   ```

4. **Kompilacja i test:**

### Oczekiwane logi:
```
🧪 TEST: Symulacja zawieszenia HTTP client...
⏱️ Ustawiono emergency timeout na 30s
🚨 EMERGENCY TIMEOUT AKTYWOWANY!
🚨 Przerwanie wszystkich operacji HTTP!
[HTTP] Emergency abort flag SET!
```

### Oczekiwany rezultat:
- Emergency timeout przerywa operację
- System odzyskuje kontrolę
- Brak watchdog reboot

## Test 3: Watchdog Reboot (Hardcore)

### Cel:
Przetestować hardware watchdog przy całkowitym zawieszeniu.

### Kroki:
1. **Wyłącz emergency system:**
   ```c
   // W main.c zakomentuj sprawdzenia:
   /*
   if (check_emergency_timeout()) {
       printf("🚨 EMERGENCY TIMEOUT - przerywam operację!\n");
       abort_all_http_operations();
       return;
   }
   */
   ```

2. **Symuluj zawieszenie głównej pętli:**
   ```c
   // W main.c dodaj w while(true):
   while (true) {
       // Zakomentuj watchdog_update();
       // watchdog_update();
       
       // Dodaj zawieszenie po 10 sekundach:
       static bool hang_triggered = false;
       if (!hang_triggered && (now > 10000)) {
           hang_triggered = true;
           printf("🧪 TEST: Symulacja zawieszenia głównej pętli...\n");
           while(true) { tight_loop_contents(); }
       }
   }
   ```

### Oczekiwane logi (przed restart):
```
🧪 TEST: Symulacja zawieszenia głównej pętli...
[ostatni log przed zawieszeniem]
```

### Oczekiwane logi (po restart):
```
🚨 WATCHDOG REBOOT - poprzedni restart przez watchdog!
🛡️ Watchdog włączony (60s timeout)
```

### Oczekiwany rezultat:
- Po ~60 sekundach: automatyczny restart
- System startuje z informacją o watchdog reboot

## Test 4: Stress Test - Długotrwałe problemy z siecią

### Cel:
Przetestować stabilność przy długotrwałych problemach sieciowych.

### Kroki:
1. **Przywróć normalny kod** (bez testowych modyfikacji)

2. **Uruchom termostat z poprawnym WiFi**

3. **Podczas działania:**
   - Wyłącz router WiFi na 2 minuty
   - Włącz router WiFi
   - Powtórz kilka razy

4. **Obserwuj logi recovery:**
   ```
   [CYW43] Sprawdzanie zdrowia CYW43...
   [CYW43] ❌ Brak połączenia WiFi - próba reconnect
   [WIFI] Próba połączenia 1/2
   [BOOT] WiFi połączony pomyślnie na próbie 1
   ```

### Oczekiwany rezultat:
- Automatyczne odzyskiwanie połączenia WiFi
- Brak zawieszenia systemu
- Prawidłowa praca termostatu offline/online

## Test 5: Endpoint Stress Test

### Cel:
Przetestować zachowanie przy obciążeniu HTTP requestów.

### Kroki:
1. **Zmniejsz odstępy między requestami:**
   ```c
   // W main.c znajdź i zmień:
   if (now - last_net > 10000) {  // było 10s
   // Na:
   if (now - last_net > 2000) {   // będzie 2s
   ```

2. **Uruchom serwer mock z opóźnieniami**

3. **Obserwuj stabilność przez 30 minut**

### Oczekiwany rezultat:
- System powinien być stabilny
- Emergency timeouts mogą występować częściej
- Brak watchdog rebootów

## Interpretacja wyników testów

### ✅ Test PASSED jeśli:
- System nigdy nie zawiesza się na dłużej niż 60 sekund
- Emergency timeouts działają poprawnie (30s dla startup, 15s dla POST)
- Po watchdog reboot system startuje normalnie
- Interfejs użytkownika pozostaje responsywny
- Thermostat działa lokalnie nawet przy problemach z siecią

### ❌ Test FAILED jeśli:
- System zawiesza się bez watchdog reboot
- Emergency timeout nie przerywa operacji HTTP
- Częste watchdog rebooty (>1 na godzinę w normalnych warunkach)
- Interfejs użytkownika nie reaguje
- Thermostat nie działa w trybie offline

## Diagnostyka problemów

### Problem: Emergency timeout nie działa
**Debug steps:**
1. Sprawdź czy `check_emergency_timeout()` jest wywoływane
2. Sprawdź czy `absolute_time_diff_us()` zwraca prawidłowe wartości
3. Dodaj więcej logów w `set_emergency_timeout()`

### Problem: HTTP client nie przerywa operacji
**Debug steps:**
1. Sprawdź czy `g_emergency_abort` jest ustawiane
2. Sprawdź czy pętla `run_client()` sprawdza flagę
3. Dodaj logi w pętli HTTP client

### Problem: Watchdog nie resetuje systemu
**Debug steps:**
1. Sprawdź czy watchdog jest włączony: `watchdog_enable(60000, 1)`
2. Sprawdź czy nie ma ukrytych `watchdog_update()` wywołań
3. Zmniejsz timeout watchdog do testów: `watchdog_enable(10000, 1)`

### Problem: Częste false positive watchdog rebooty
**Solutions:**
1. Zwiększ timeout watchdog: `watchdog_enable(120000, 1)` (2 minuty)
2. Sprawdź czy `watchdog_update()` jest w głównej pętli
3. Sprawdź czy nie ma długich operacji blokujących

## Rollback po testach

**Przed wgraniem produkcyjnego firmware:**

1. **Wyłącz test mode:**
   ```c
   // Zakomentuj w main.c i hb_http.c:
   // #define EMERGENCY_TEST_MODE
   ```

2. **Przywróć poprawny adres serwera:**
   ```c
   #define HB_HOST "192.168.55.119"  // Twój rzeczywisty serwer
   ```

3. **Przywróć standardowe timeouty:**
   ```c
   if (now - last_net > 10000) {  // 10 sekund między requestami
   ```

4. **Usuń testowe modyfikacje kodu**

5. **Kompiluj i wgraj clean firmware**

## Automatyzacja testów (opcjonalne)

**Python script do testowania:**

```python
#!/usr/bin/env python3
import serial
import time
import re

def test_emergency_timeout():
    ser = serial.Serial('/dev/ttyACM0', 115200, timeout=1)
    
    # Szukaj logów emergency timeout
    start_time = time.time()
    while time.time() - start_time < 45:  # 45s timeout
        line = ser.readline().decode().strip()
        if "EMERGENCY TIMEOUT AKTYWOWANY" in line:
            print("✅ Emergency timeout test PASSED")
            return True
        if line:
            print(line)
    
    print("❌ Emergency timeout test FAILED")
    return False

if __name__ == "__main__":
    test_emergency_timeout()
```

Ten system testów zapewnia pełną weryfikację zabezpieczeń przed wdrożeniem w środowisku produkcyjnym.