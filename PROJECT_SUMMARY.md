# HeatBeat Thermostat - System Summary

## 🎯 Cel projektu
Inteligentny termostat z dwukierunkową komunikacją z serwerem centralnym, umożliwiający zmianę temperatury zarówno lokalnie (na urządzeniu) jak i zdalnie (przez aplikację mobilną).

## 🏗️ Architektura systemu

### Hardware:
- **Raspberry Pi Pico W** - mikrokontroler z WiFi
- **BME280** - czujnik temperatury, wilgotności i ciśnienia
- **Wyświetlacz LCD z touchem** - interfejs użytkownika LVGL
- **Przekaźnik ogrzewania** - sterowanie grzaniem

### Software Stack:
- **LVGL** - biblioteka GUI z kontrolą arc dla temperatury
- **lwIP** - stos TCP/IP (raw API, NO_SYS mode)
- **HTTP Client** - własna implementacja dla komunikacji z serwerem
- **JSON Parser** - parsowanie odpowiedzi serwera
- **Emergency Recovery** - wielopoziomowy system zabezpieczeń

## 🌐 Protokół komunikacji

### API Endpoints:
```
GET  /device/{id}/settings     # Pobierz ustawienia temperatury
POST /device/{id}/reading      # Wyślij odczyty czujników
PUT  /device/{id}/settings     # Zaktualizuj temperaturę [opcjonalne]
```

### JSON Format:
```json
// GET /settings response:
{
  "target_temp_c": 21.5,
  "last_source": "mobile_app",
  "timestamp": 1703123456
}

// POST /reading request:
{
  "temperature": 20.8,
  "humidity": 45.2,
  "pressure": 1013.25,
  "target_temp_c": 21.5,
  "timestamp": 1703123456,
  "device_id": 1
}
```

## 🔧 Kluczowe funkcje

### 1. Dwukierunkowa komunikacja:
- **Lokalne zmiany** → wysyłane na serwer via POST /reading
- **Zdalne zmiany** → pobierane via GET /settings
- **Konflikt resolution** - priorytet ostatniej zmiany

### 2. Interfejs użytkownika:
- **Arc control** - intuicyjna zmiana temperatury dotykiem
- **Status notifications** - informacje o stanie połączenia
- **WiFi indicator** - siła sygnału i status połączenia
- **External change indicator** - powiadomienie o zdalnych zmianach

### 3. Offline capability:
- **Standalone operation** - termostat działa bez internetu
- **Local temperature control** - niezależnie od serwera
- **Automatic reconnection** - powrót do komunikacji po odzyskaniu łączności

## 🛡️ Emergency Recovery System

### Poziom 1: Application Timeouts
```c
set_emergency_timeout(30000);  // 30s dla sync przy starcie
set_emergency_timeout(15000);  // 15s dla POST reading

if (check_emergency_timeout()) {
    abort_all_http_operations();
    return;  // Kontynuuj offline
}
```

### Poziom 2: HTTP Client Abort
```c
// W pętli run_client():
if (g_emergency_abort) {
    printf("🚨 EMERGENCY ABORT - przerywam loop!\n");
    c->state = ST_ERROR;
    break;
}
```

### Poziom 3: Hardware Watchdog
```c
watchdog_enable(60000, 1);     // 60s timeout
// W głównej pętli:
watchdog_update();             // Przedłuż watchdog
```

## 📊 Monitoring i diagnostyka

### Debug Logging:
```
[STARTUP] ⏱️ Ustawiono emergency timeout na 30s
[HTTP] run_client: loop 1000, state=2
[CYW43] Sprawdzanie zdrowia CYW43...
🚨 EMERGENCY TIMEOUT AKTYWOWANY!
🛡️ Watchdog włączony (60s timeout)
```

### Metryki systemu:
- Emergency timeout count
- Watchdog reboot count  
- HTTP success/failure rate
- WiFi connection stability

## 🔄 Przepływ działania

### 1. Startup Sequence:
```
1. Watchdog enable (60s)
2. WiFi connection (multi-retry)
3. GUI initialization
4. Server synchronization (30s timeout)
5. Main loop start
```

### 2. Main Loop (każdy cykl ~10ms):
```
1. watchdog_update()
2. LVGL timer handler
3. BME280 reading (co 2s)
4. Server communication (co 10s)
5. UI updates
```

### 3. Temperature Change Flow:
```
Local change:
1. User touches arc control
2. Temperature updated locally
3. POST reading to server (15s timeout)
4. Reset emergency state on success

Remote change:
1. GET settings from server (10s intervals)
2. Compare with local temperature
3. Update UI if different
4. Show "external change" notification
```

### 4. Error Recovery:
```
WiFi error:
1. CYW43 health check (co 30s)
2. Multi-retry reconnection
3. UI status update

HTTP timeout:
1. Emergency timeout triggers (15-30s)
2. Abort HTTP operation
3. Continue offline operation
4. Retry in next cycle

System hang:
1. Watchdog timeout (60s)
2. Hardware reset
3. System restart with reboot detection
```

## 📁 Struktura kodu

### Core Files:
```
main.c                     # Main logic, WiFi, emergency system
├── startup_sync_with_server()
├── send_reading_now()
├── emergency timeout functions
└── main loop with watchdog

net/hb_http.c             # HTTP client implementation
├── run_client()          # Main TCP loop
├── on_connected/sent/recv/err callbacks
└── emergency abort handling

net/hb_proto.h            # JSON protocol definitions
├── hb_build_http_*()     # HTTP request builders
├── hb_parse_*()          # JSON response parsers
└── Protocol structures

lvgl_ui/main_screen.c     # User interface
├── arc_event_cb()        # Temperature control
├── notification system
└── status indicators
```

### Support Files:
```
bme280_port.c             # Sensor interface
bsp_*.c                   # Board support package
Emergency_Recovery_System.md    # System documentation
Testing_Emergency_System.md     # Test procedures
API_Documentation.md            # Complete API spec
```

## 🚀 Deployment Checklist

### Pre-deployment:
- [ ] All tests passed (emergency timeout, HTTP abort, watchdog)
- [ ] Production server configuration verified
- [ ] WiFi credentials updated
- [ ] Test mode disabled (`#define EMERGENCY_TEST_MODE`)
- [ ] Debug logging appropriate for production

### Post-deployment monitoring:
- [ ] Monitor watchdog reboot frequency (<1/hour)
- [ ] Monitor emergency timeout frequency
- [ ] Monitor HTTP success rate (>90%)
- [ ] Monitor WiFi stability

### Success criteria:
- [ ] System never hangs longer than 60 seconds
- [ ] Thermostat operates offline when needed
- [ ] UI remains responsive
- [ ] Temperature control works locally and remotely
- [ ] Automatic recovery from network issues

## 🔍 Troubleshooting Quick Reference

### System hangs during startup:
1. Check emergency timeout logs (30s limit)
2. Verify server accessibility
3. Check WiFi connection stability

### Frequent watchdog reboots:
1. Verify `watchdog_update()` in main loop
2. Check for blocking operations >60s
3. Consider increasing watchdog timeout

### HTTP communication fails:
1. Check emergency timeout configuration
2. Verify HTTP client abort functionality
3. Test offline operation capability

### UI not responsive:
1. Check LVGL timer handler execution
2. Verify main loop timing
3. Check for blocking operations

## 📈 Performance Characteristics

### Timing:
- **UI responsiveness**: 10ms (LVGL_TICK_MS)
- **Sensor reading**: 2 seconds
- **Server sync**: 10 seconds  
- **Emergency timeout**: 15-30 seconds
- **Watchdog timeout**: 60 seconds

### Memory usage:
- **HTTP buffer**: 1024 bytes
- **JSON parsing**: Stack-based
- **LVGL**: Managed buffers
- **Total RAM**: ~200KB available

### Network:
- **HTTP keepalive**: No (one-shot connections)
- **Connection timeout**: 10 seconds
- **Request size**: <500 bytes
- **Response size**: <200 bytes

## 🎯 Achievement Summary

✅ **Podstawowa funkcjonalność termostatu** - sterowanie lokalnie i zdalnie  
✅ **Dwukierunkowa komunikacja** - synchronizacja z serwerem  
✅ **Interfejs użytkownika** - LVGL z arc control i notyfikacjami  
✅ **System emergency recovery** - wielopoziomowe zabezpieczenia  
✅ **Offline capability** - praca niezależna od serwera  
✅ **Automatyczne odzyskiwanie** - reconnect WiFi i HTTP  
✅ **Pełna dokumentacja** - API, testy, troubleshooting  
✅ **Testowanie** - procedury weryfikacji wszystkich scenariuszy  

Termostat jest gotowy do wdrożenia produkcyjnego z pełną niezawodnością i bezpieczeństwem działania.