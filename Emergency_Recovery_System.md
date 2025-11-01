# Emergency Recovery System - HeatBeat Thermostat

## System wielopoziomowego zabezpieczenia przed zawieszeniem

### Poziom 1: Emergency Timeout (main.c)
**Cel:** Zabezpieczenie przed zawieszeniem podczas operacji HTTP w warstwie aplikacyjnej

**Mechanizm:**
- `set_emergency_timeout(timeout_ms)` - ustawia timeout na operację HTTP
- `check_emergency_timeout()` - sprawdza czy minął timeout
- `abort_all_http_operations()` - przerywa wszystkie operacje HTTP

**Timeouty:**
- Synchronizacja przy starcie: 30 sekund
- POST reading: 15 sekund

**Logika działania:**
```c
// Przed operacją HTTP
set_emergency_timeout(30000);  // 30s dla GET settings

// Podczas operacji HTTP
if (check_emergency_timeout()) {
    printf("🚨 EMERGENCY TIMEOUT - przerywam operację!\n");
    abort_all_http_operations();
    return;
}

// Po udanej operacji
reset_emergency_state();
```

### Poziom 2: HTTP Client Emergency Abort (hb_http.c)
**Cel:** Przerwanie operacji HTTP na poziomie klienta TCP

**Mechanizm:**
- `hb_http_set_emergency_abort(bool abort)` - ustawia flagę abort
- `hb_http_is_emergency_abort()` - sprawdza flagę abort
- Sprawdzenie w głównej pętli `run_client()`:

```c
while (c->state != ST_DONE && c->state != ST_ERROR) {
    // Sprawdź emergency abort flag
    if (g_emergency_abort) {
        printf("🚨 EMERGENCY ABORT - przerywam loop!\n");
        c->err = HB_HTTP_ERR_TIMEOUT;
        c->state = ST_ERROR;
        break;
    }
    // ... reszta pętli
}
```

### Poziom 3: Hardware Watchdog Timer
**Cel:** Ostateczne zabezpieczenie przed całkowitym zawieszeniem systemu

**Konfiguracja:**
- Timeout: 60 sekund
- Aktywacja przy starcie: `watchdog_enable(60000, 1)`
- Odświeżanie w głównej pętli: `watchdog_update()`

**Detekcja restartów:**
```c
if (watchdog_caused_reboot()) {
    printf("🚨 WATCHDOG REBOOT - poprzedni restart przez watchdog!\n");
}
```

## Przepływ działania Emergency System

### Normalny scenariusz:
1. `set_emergency_timeout(30000)` - ustaw timeout
2. `hb_http_get_settings()` - wykonaj operację HTTP
3. `reset_emergency_state()` - resetuj po sukcesie
4. `watchdog_update()` - regularnie w głównej pętli

### Scenariusz timeout:
1. `check_emergency_timeout()` wykrywa timeout
2. `abort_all_http_operations()` ustawia flagi abort
3. `hb_http_set_emergency_abort(true)` przerywa HTTP client
4. HTTP client kończy z błędem `HB_HTTP_ERR_TIMEOUT`
5. Aplikacja kontynuuje pracę w trybie offline

### Scenariusz całkowitego zawieszenia:
1. Brak `watchdog_update()` przez 60 sekund
2. Hardware watchdog resetuje system
3. Po restarcie: `watchdog_caused_reboot()` = true
4. System uruchamia się ponownie z czystym stanem

## Diagnostyka i monitorowanie

### Logi Emergency Timeout:
```
⏱️ Ustawiono emergency timeout na 30s
🚨 EMERGENCY TIMEOUT AKTYWOWANY!
🚨 Przerwanie wszystkich operacji HTTP!
```

### Logi HTTP Client Abort:
```
[HTTP] Emergency abort flag SET!
[HTTP] run_client: 🚨 EMERGENCY ABORT - przerywam loop!
```

### Logi Watchdog:
```
🛡️ Watchdog włączony (60s timeout)
🚨 WATCHDOG REBOOT - poprzedni restart przez watchdog!
```

### Logi Reset Emergency State:
```
✅ Emergency state zresetowany
```

## Konfiguracja timeoutów

### Parametry można dostosować w main.c:
```c
// Emergency timeouts
set_emergency_timeout(30000);  // Startup sync: 30s
set_emergency_timeout(15000);  // POST reading: 15s

// Watchdog timeout
watchdog_enable(60000, 1);     // Hardware watchdog: 60s

// HTTP Client timeout
#define CONNECTION_TIMEOUT_MS 10000  // TCP timeout: 10s
```

## Testowanie Emergency System

### Test 1: Symulacja timeoutu HTTP
1. Odłącz serwer HTTP
2. Uruchom termostat
3. Sprawdź czy po 30s następuje emergency timeout
4. Sprawdź czy system kontynuuje pracę offline

### Test 2: Symulacja zawieszenia HTTP client
1. Zmodyfikuj kod aby zawiesić pętlę w `run_client()`
2. Sprawdź czy emergency abort przerywa operację
3. Sprawdź czy system odzyskuje kontrolę

### Test 3: Symulacja całkowitego zawieszenia
1. Zmodyfikuj kod aby zawiesić główną pętlę
2. Sprawdź czy po 60s następuje watchdog reset
3. Sprawdź czy system restartuje poprawnie

## Troubleshooting

### Problem: System się wiesza mimo emergency timeout
**Rozwiązanie:** Sprawdź czy `check_emergency_timeout()` jest wywoływane regularnie

### Problem: HTTP client nie przerywa operacji
**Rozwiązanie:** Sprawdź czy `g_emergency_abort` jest sprawdzane w pętli `run_client()`

### Problem: Częste watchdog rebooty
**Rozwiązanie:** 
1. Sprawdź czy `watchdog_update()` jest wywoływane w głównej pętli
2. Zwiększ timeout watchdog jeśli operacje trwają dłużej niż 60s
3. Sprawdź czy nie ma nieskończonych pętli

### Problem: Emergency timeout nie działa
**Rozwiązanie:**
1. Sprawdź czy timeout jest ustawiony przed operacją HTTP
2. Sprawdź czy `absolute_time_diff_us()` jest używane poprawnie
3. Sprawdź czy flaga `emergency_abort` jest sprawdzana

## Metryki i monitoring

### Liczyć należy:
- Liczbę emergency timeoutów
- Liczbę watchdog rebootów  
- Czas trwania operacji HTTP
- Liczbę udanych/nieudanych operacji HTTP

### Przykład rozszerzenia o metryki:
```c
static uint32_t emergency_timeout_count = 0;
static uint32_t watchdog_reboot_count = 0;
static uint32_t http_success_count = 0;
static uint32_t http_fail_count = 0;

// W check_emergency_timeout():
if (timeout) {
    emergency_timeout_count++;
}

// W main():
if (watchdog_caused_reboot()) {
    watchdog_reboot_count++;
}
```

## Wnioski

System wielopoziomowego zabezpieczenia zapewnia:
1. **Responsywność** - szybkie przerwanie operacji HTTP (sekundy)
2. **Niezawodność** - system zawsze odzyskuje kontrolę
3. **Diagnostyka** - pełne logowanie problemów
4. **Stabilność** - hardware watchdog jako ostateczna linia obrony

Ten system zapewnia że termostat nigdy nie zawiesi się na dłużej niż 60 sekund i zawsze będzie działał w trybie lokalnym, nawet przy problemach z komunikacją sieciową.