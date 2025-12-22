# HeatBeat Thermostat - README

## 🎯 Opis Projektu
Inteligentny termostat oparty na **Raspberry Pi Pico W** z dwukierunkową komunikacją z serwerem centralnym. System umożliwia kontrolę temperatury lokalnie (na urządzeniu) oraz zdalnie (przez aplikację mobilną/webową).

## ✨ Kluczowe Funkcje

### 🌐 **Komunikacja Sieciowa**
- **GET** `/device/{id}/settings` - pobieranie ustawień co 5 sekund
- **POST** `/device/{id}/reading` - wysyłanie telemetrii co 30 sekund
- **PUT** `/device/{id}/settings` - aktualizacja ustawień [opcjonalne]
- **WiFi Multi-Auth** - obsługa różnych metod autoryzacji
- **Emergency Recovery** - wielopoziomowe zabezpieczenia HTTP

### 🖥️ **Interfejs Użytkownika**
- **LVGL GUI** z kontrolą arc dla temperatury
- **Odblokowywanie ekranu** - 3 szybkie dotknięcia
- **Status systemu** - WiFi, Backend cache, Local override
- **Powiadomienia** - informacje o zmianach z serwera
- **Język polski** - wszystkie komunikaty w języku polskim

### 🌡️ **Zarządzanie Temperaturą**
- **BME280** - pomiary temperatury, wilgotności, ciśnienia
- **Kontrola przekaźnika** - sterowanie systemem grzewczym
- **Wykrywanie otwartego okna** - spadek ≥2°C w ciągu 2 minut
- **Alarm dźwiękowy** - ostrzeżenie o otwartym oknie
- **Lokalne nadpisanie** - priorytet zmian lokalnych

### 🔒 **Bezpieczeństwo i Stabilność**
- **HTTP Mutex** - zabezpieczenie przed nakładającymi się requestami
- **Timeout mechanizmy** - Emergency timeout (1M iteracji) + Progress timeout (2-4s)
- **CYW43 Diagnostyka** - diagnostyka modułu WiFi przy starcie
- **Watchdog** - hardware watchdog dla systemu
- **Offline Mode** - kontynuacja pracy bez połączenia

## 🏗️ Architektura

```
┌─────────────────┐    HTTP/WiFi    ┌─────────────────┐
│   HeatBeat      │ ←────────────→  │ Serwer Backend  │
│   Thermostat    │                 │                 │
│   (Pico W)      │                 │  - Ustawienia   │
└─────────────────┘                 │  - Telemetria   │
         ↑                          │  - API REST     │
    ┌────┴────┐                     └─────────────────┘
    │         │                              ↑
┌───▼───┐ ┌───▼────┐                        │
│BME280 │ │ LVGL   │                   ┌────▼────┐
│Sensor │ │ Touch  │                   │Frontend │
└───────┘ └────────┘                   │Web/App  │
                                       └─────────┘
```

## 📋 Przypadki Użycia

### **Aktorzy:**
- **Użytkownik** - obsługa interfejsu dotykowego
- **System Backendu** - zarządzanie ustawieniami  
- **Czujnik BME280** - pomiary środowiskowe
- **System Grzewczy** - odbieranie sygnałów sterujących

### **Główne Funkcjonalności:**
- **UC1:** Wyświetlanie temperatury i wilgotności
- **UC2:** Ustawianie temperatury docelowej
- **UC3:** Odblokowywanie ekranu (3 dotknięcia)
- **UC4:** Kontrola przekaźnika grzewczego
- **UC5:** Wykrywanie i alarm otwartego okna
- **UC6:** Komunikacja z backendem
- **UC7:** Synchronizacja ustawień
- **UC8:** Diagnostyka systemu

*Zobacz szczegółowy diagram: [docs/use_cases.puml](docs/use_cases.puml)*

## 🔧 Konfiguracja

### **WiFi:**
```c
#define WIFI_SSID "YourNetwork"
#define WIFI_PASS "YourPassword"
```

### **Backend:**
```c
#define HB_HOST "your-backend.com"
#define HB_PORT 80
#define HB_DEVICE_ID 1
```

### **Interwały:**
```c
#define SETTINGS_CHECK_INTERVAL_MS  5000   // GET co 5s
#define READING_SEND_INTERVAL_MS    30000  // POST co 30s
#define TELEMETRY_INTERVAL_MS       30000  // Telemetria co 30s
```

## 📊 Protokół API

### **GET /device/{id}/settings**
```json
{
  "target_temp_c": 21.5,
  "last_source": "app",
  "timestamp": 1703123456
}
```

### **POST /device/{id}/reading**
```json
{
  "temperature": 20.8,
  "humidity": 45.2,
  "pressure": 1013.25,
  "target_temp_c": 21.5,
  "window_open": false,
  "timestamp": 1703123456
}
```

## 🛠️ Kompilacja

### **Wymagania:**
- Pico SDK
- CMake
- Ninja/Make

### **Budowanie:**
```bash
mkdir build && cd build
cmake ..
make -j4
```

### **Programowanie:**
```bash
# Tryb BOOTSEL
picotool load heatbeat.uf2

# Przez SWD/OpenOCD  
openocd -f interface/cmsis-dap.cfg -f target/rp2040.cfg \
        -c "program heatbeat.elf verify reset exit"
```

## 🐛 Diagnostyka

### **Logi Systemowe:**
```
[BOOT] 🔧 NAJPIERW DIAGNOSTYKA CYW43...
[TEST 1] SUKCES: Podstawowa inicjalizacja OK
[WiFi] SUKCES! Połączono z "Network"
[STATUS] WiFi: Połączony, Cache backendu: Tak, Nadpisanie lokalne: Nieaktywny
```

### **Monitoring HTTP:**
```
[HTTP DEBUG] teraz=12345, ostatni_post=12000, różnica=345, interwał=30000
[NET] GET /device/1/ustawienia
[NET] POST odczyt: temp=21.2°C, zadana=21.5°C, okno_otwarte=nie
```

### **Wykrywanie Problemów:**
- **Zawieszenia HTTP** - Emergency timeout + Progress monitoring
- **Problemy WiFi** - CYW43 diagnostyka + Recovery system
- **Braki pamięci** - RAM monitoring z logowaniem
- **Błędy komunikacji** - Retry logic + Offline fallback

## 📁 Struktura Plików

```
HeatBeat-Pico/
├── firmware/lvgl/heatbeat/
│   ├── main.c                  # Główna logika aplikacji
│   ├── net/hb_http.c          # Klient HTTP
│   ├── net/hb_proto.h         # Protokół API
│   ├── lvgl_ui/main_screen.c  # Interfejs LVGL
│   └── bme280_port.c          # Sterownik czujnika
├── docs/
│   └── use_cases.puml         # Diagram przypadków użycia
├── libraries/                 # Biblioteki zewnętrzne
│   ├── lvgl/                 # Biblioteka GUI
│   └── bsp/                  # Board Support Package
└── README.md                 # Ten plik
```

## 🆕 Najnowsze Funkcje

### **v1.2.0 - Grudzień 2024:**
- ✅ **Polskie logi systemowe** - wszystkie komunikaty w języku polskim
- ✅ **Poprawiona synchronizacja** - usunięto blokady GET requestów
- ✅ **Emergency timeouts** - zabezpieczenia przed zawieszaniem HTTP
- ✅ **Diagnostyka WiFi** - szczegółowe testy modułu CYW43
- ✅ **Wykrywanie okien** - alarm przy spadku temperatury
- ✅ **Optymalizacja pamięci** - monitoring i zarządzanie RAM

### **Planowane Funkcje:**
- 🔄 **HTTPS** - szyfrowana komunikacja z backendem
- 🔄 **NTP Sync** - synchronizacja czasu z serwerem
- 🔄 **OTA Updates** - aktualizacje firmware przez sieć
- 🔄 **Harmonogram** - programowalna kontrola temperatury

## 📞 Wsparcie

**Autor:** suma-soft  
**Repo:** [HeatBeat-Pico](https://github.com/suma-soft/HeatBeat-Pico)  
**Licencja:** MIT

---

*Inteligentny termostat dla nowoczesnego domu 🏠*