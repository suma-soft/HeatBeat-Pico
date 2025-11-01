# Termostat Inteligentny - HeatBeat-Pico

## Opis
Inteligentny termostat oparty na Raspberry Pi Pico W z dwukierunkową komunikacją z serwerem centralnym. Użytkownik może zmieniać temperaturę zarówno lokalnie na termostacie, jak i zdalnie przez aplikację mobilną.

## Cechy Główne

### 🌐 Komunikacja Dwukierunkowa
- **Lokalne zmiany**: Natychmiastowe wysyłanie na serwer przez PUT/POST
- **Zdalne zmiany**: Automatyczne pobieranie z serwera co 8 sekund
- **Konflikt handling**: Ochrona lokalnych zmian przez 5 sekund
- **Offline mode**: Kontynuacja pracy bez połączenia z internetem

### 📱 Interfejs Użytkownika
- **Łuk kontrolny**: Intuicyjna zmiana temperatury dotykiem
- **Status WiFi**: Ikona z sygnalizacją siły sygnału
- **Powiadomienia**: Informacje o zmianach z aplikacji
- **Status połączenia**: Komunikaty o stanie komunikacji

### 🔄 Synchronizacja Inteligentna
- **Startup sync**: Pobieranie ustawień przy starcie
- **Source tracking**: Rozróżnianie zmian z aplikacji vs urządzenia
- **Retry logic**: Automatyczne ponowne próby przy błędach
- **Cache lokalny**: Zapamiętywanie ostatnich ustawień

## Architektura

### Komponenty
```
┌─────────────────┐    HTTP/WiFi    ┌─────────────────┐
│   Termostat     │ ←────────────→  │ Serwer Central  │
│  (Pico W)      │                 │                 │
└─────────────────┘                 └─────────────────┘
         ↑                                   ↑
    Sensors/UI                        Aplikacja Mobilna
```

### Pliki Kluczowe
- `main.c` - Główna logika termostatu i komunikacji
- `hb_http.c/h` - Klient HTTP do komunikacji z serwerem  
- `hb_proto.h` - Protokół komunikacji i formaty JSON
- `main_screen.c/h` - Interfejs użytkownika LVGL
- `thermostat_config.h` - Konfiguracja parametrów

## Konfiguracja

### Parametry WiFi
```c
#define WIFI_SSID "YourWiFiNetwork" 
#define WIFI_PASS "YourPassword"
```

### Parametry Serwera
```c
#define HB_HOST "192.168.55.119"
#define HB_PORT 8000
#define HB_DEVICE_ID 1
```

### Parametry Czasowe
```c
#define SERVER_CHECK_INTERVAL_MS    8000   // Sprawdzanie serwera
#define READING_SEND_INTERVAL_MS    15000  // Wysyłanie odczytów  
#define LOCAL_OVERRIDE_WINDOW_MS    5000   // Ochrona lokalna
#define CONNECTION_TIMEOUT_MS       4000   // Timeout HTTP
```

## API Serwera

### Endpointy
- `GET /device/{id}/settings` - Pobierz ustawienia
- `PUT /device/{id}/settings` - Ustaw temperaturę 
- `POST /device/{id}/reading` - Wyślij odczyty

### Formaty JSON

**Pobieranie ustawień (GET response):**
```json
{
  "target_temp_c": 22.5,
  "last_source": "app"
}
```

**Ustawianie temperatury (PUT request):**
```json
{
  "target_temp_c": 23.0,
  "source": "device"  
}
```

**Wysyłanie odczytów (POST request):**
```json
{
  "temperature_c": 21.5,
  "humidity_pct": 45.2,
  "pressure_hpa": 1013.25,
  "setpoint_c": 22.0
}
```

## Logika Biznesowa

### Scenariusze Główne

**1. Startup Termostatu**
1. Połącz z WiFi (timeout 15s)
2. Pobierz ustawienia z serwera  
3. Ustaw temperaturę na interfejsie
4. Uruchom cykliczne sprawdzanie

**2. Zmiana Lokalna**
1. Użytkownik zmienia temperaturę na łuku
2. Aktywacja ochrony lokalnej (5s)
3. Próba PUT /settings
4. Jeśli błąd → wysyłka przez POST /reading
5. Powiadomienie użytkownika

**3. Zmiana Zdalna**  
1. Serwer co 8s sprawdza GET /settings
2. Jeśli `last_source: "app"` → aktualizuj UI
3. Pokaż ikonę telefonu i powiadomienie
4. Ignoruj podczas ochrony lokalnej

### Obsługa Źródeł
- `"app"` → Zmiana z aplikacji mobilnej
- `"device"` → Zmiana z termostatu (ignoruj)
- `null` → Źródło nieznane

## Komunikaty Użytkownika

### Standardowe
- ✅ "Połączono z serwerem"
- ✅ "Temperatura zaktualizowana"  
- ✅ "Wysłano do aplikacji"

### Zmiany Zewnętrzne
- 📱 "Nowa temperatura z aplikacji: 22.5°C"
- 🔄 "Ustawienia zmienione zdalnie"

### Błędy
- ❌ "Brak połączenia z serwerem"
- ❌ "Nie udało się wysłać danych"
- ⚠️ "Błąd komunikacji - pracuję offline"

## Kompilacja

### Wymagania
- Raspberry Pi Pico SDK
- LVGL library  
- lwIP stack
- BME280 sensor library

### Budowanie
```bash
mkdir build && cd build
cmake ..
make
```

### Flashowanie
```bash
# Metoda 1: Picotool
picotool load firmware.uf2 -fx

# Metoda 2: BOOTSEL mode
# 1. Przytrzymaj BOOTSEL i podłącz USB
# 2. Skopiuj firmware.uf2 na dysk RPI-RP2
```

## Testowanie

### Test Lokalny
1. Uruchom termostat
2. Zmień temperaturę na łuku  
3. Sprawdź komunikaty statusu
4. Zweryfikuj logi seria

### Test Zdalny
1. Zmień temperaturę w aplikacji
2. Sprawdź aktualizację na termostacie
3. Zweryfikuj ikonę telefonu
4. Sprawdź powiadomienia

### Test Offline
1. Wyłącz WiFi/internet
2. Zmień temperaturę lokalnie
3. Sprawdź tryb offline
4. Włącz połączenie - sprawdź sync

## Rozwiązywanie Problemów

### Brak Połączenia WiFi
- Sprawdź SSID i hasło w kodzie
- Zweryfikuj zasięg WiFi
- Sprawdź logi startu
- Reset termostatu

### Błędy Komunikacji
- Sprawdź adres IP serwera
- Zweryfikuj port (8000)
- Sprawdź firewall
- Test ping do serwera

### Problemy UI
- Sprawdź kalibragę dotyka
- Zweryfikuj LVGL init
- Sprawdź logi UART
- Reset LVGL

## Monitorowanie

### Logi Systemowe
```
[BOOT] HeatBeat-Pico start
[WiFi] Połączono z "YourNetwork" IP:192.168.55.100
[NET] target 192.168.55.119:8000 dev=1
[SYNC] Temperatura zmieniona z aplikacji: 22.5°C
[STATUS] WiFi: Connected, Backend cache: Yes, Local override: Inactive
```

### Debug UART
- Baud rate: 115200
- Data: 8N1
- Terminal: PuTTY/minicom

## Rozszerzenia

### Dodatkowe Funkcje
- ✨ Cache lokalny w EEPROM  
- ✨ OTA Updates przez WiFi
- ✨ Historia zmian temperatury
- ✨ Harmonogram tygodniowy
- ✨ Multiple sensor support
- ✨ Voice control integration

### Custom Endpoints
- Health check endpoint
- Diagnostic data
- Firmware version
- WiFi signal strength

## Licencja
MIT License - patrz LICENSE file

## Autorzy
- Suma-Soft Development Team
- Raspberry Pi Pico Community