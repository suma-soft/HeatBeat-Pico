# Historia Zmian - HeatBeat Thermostat

## v1.3.0 - Grudzień 2024 🇵🇱

### ✨ Nowe Funkcje
- **Kompletna polonizacja** - wszystkie logi systemowe w języku polskim
- **Diagram przypadków użycia** - szczegółowa dokumentacja PlantUML
- **Ulepszona diagnostyka CYW43** - 6-etapowe testy modułu WiFi przy starcie
- **Wykrywanie otwartych okien** - alarm przy spadku temperatury ≥2°C w 2 min
- **Sygnały dźwiękowe** - informacje o włączeniu/wyłączeniu grzania

### 🔧 Poprawki Stabilności
- **Emergency timeout system** - backup timeout 1M iteracji dla HTTP
- **Progress monitoring** - timeout 2s dla connecting, 4s inne operacje  
- **HTTP Mutex protection** - zabezpieczenie przed nakładającymi się requestami
- **Usunięcie blokad GET** - synchronizacja temperatury działa co 5s
- **CYW43 Recovery** - automatyczne odzyskiwanie przy błędach WiFi

### 📊 Optymalizacje
- **Częstotliwość GET** - zmiana z 8s na 5s dla szybszej synchronizacji
- **Uproszczenie logiki** - usunięcie niepotrzebnego `http_delay_ok` trackingu
- **RAM monitoring** - szczegółowe logowanie zużycia pamięci
- **Cleanup HTTP** - lepsze zarządzanie cyklem życia połączeń

### 🌡️ UI/UX Usprawnienia
- **Status w języku polskim** - "Połączony/Rozłączony", "Tak/Nie", "Aktywny/Nieaktywny"
- **Odblokowywanie ekranu** - 3 szybkie dotknięcia z polskimi komunikatami
- **Powiadomienia o oknach** - "🚨 WYKRYTO OTWARTE OKNO!"
- **Debug logi HTTP** - "[HTTP DEBUG] teraz=X, różnica=Y"

---

## v1.2.0 - Listopad 2024

### ✨ Nowe Funkcje  
- **Dwukierunkowa komunikacja** - GET/POST z backendem
- **Source tracking** - rozróżnianie zmian z 'app' vs 'device'
- **Local override** - priorytet lokalnych zmian z ochroną czasową
- **Startup synchronizacja** - pobieranie ustawień przy uruchomieniu

### 🔧 Stabilność HTTP
- **Timeout mechanizmy** - ochrona przed zawieszaniem requestów
- **Retry logic** - automatyczne ponowne próby przy błędach  
- **Cache lokalny** - zapamiętywanie ostatnich ustawień z serwera
- **Offline mode** - kontynuacja pracy bez połączenia

### 📱 Interfejs LVGL
- **Arc control** - intuicyjna kontrola temperatury dotykiem
- **Status indicators** - informacje o połączeniu i cache
- **Notifications** - komunikaty o zmianach z aplikacji
- **Touch unlock** - odblokowywanie przez dotknięcia

---

## v1.1.0 - Październik 2024

### ✨ Podstawowe Funkcje
- **BME280 integration** - odczyt temperatury, wilgotności, ciśnienia
- **Relay control** - sterowanie systemem grzewczym  
- **WiFi connectivity** - połączenie z siecią domową
- **HTTP client** - komunikacja z serwerem centralnym

### 🏗️ Architektura
- **LVGL GUI** - nowoczesny interfejs graficzny
- **lwIP stack** - stos TCP/IP w trybie raw API
- **JSON parsing** - obsługa odpowiedzi serwera
- **Modular design** - podział na komponenty funkcjonalne

### 🔒 Bezpieczeństwo  
- **Watchdog timer** - hardware watchdog dla niezawodności
- **Error handling** - obsługa błędów komunikacji
- **Safe defaults** - bezpieczne wartości domyślne

---

## v1.0.0 - Wrzesień 2024

### 🎯 Pierwsze Wydanie
- **Raspberry Pi Pico W** - platforma sprzętowa
- **Podstawowy termostat** - kontrola temperatury
- **Czujnik BME280** - pomiary środowiskowe
- **Wyświetlacz dotykowy** - interfejs użytkownika

### 📋 Funkcje MVP
- Lokalna kontrola temperatury
- Wyświetlanie pomiarów  
- Podstawowe sterowanie przekaźnikiem
- Prosty interfejs LVGL

---

## 🔮 Roadmapa Przyszłych Wersji

### v1.4.0 - Planowane na Q1 2025
- **HTTPS support** - szyfrowana komunikacja z backendem
- **NTP synchronization** - synchronizacja czasu z serwerem  
- **Configuration portal** - portal konfiguracji przez WiFi
- **Multiple sensors** - obsługa wielu czujników BME280

### v1.5.0 - Planowane na Q2 2025  
- **OTA updates** - aktualizacje firmware przez sieć
- **Scheduling system** - harmonogram temperatury
- **Energy monitoring** - monitorowanie zużycia energii
- **Mobile app integration** - dedykowana aplikacja mobilna

### v2.0.0 - Planowane na Q3 2025
- **Multi-zone support** - obsługa wielu stref grzewczych  
- **Advanced algorithms** - predykcyjne sterowanie
- **Home automation** - integracja z systemami smart home
- **Cloud analytics** - analityka w chmurze

---

## 📊 Statystyki Projektu

### 📁 Kod
- **Języki:** C (firmware), PlantUML (dokumentacja)
- **Linie kodu:** ~3000+ (główny firmware)
- **Pliki:** 20+ (bez bibliotek zewnętrznych)
- **Biblioteki:** LVGL, lwIP, Pico SDK

### 🧪 Testy
- **CYW43 diagnostyka:** 6 testów sprzętowych
- **HTTP timeouts:** Emergency + Progress monitoring  
- **RAM monitoring:** Ciągłe śledzenie pamięci
- **Error recovery:** Automatyczne odzyskiwanie z błędów

### 🌐 Komunikacja
- **GET frequency:** Co 5 sekund
- **POST frequency:** Co 30 sekund  
- **Timeout emergency:** 1M iteracji backup
- **Timeout progress:** 2-4s per operation

---

*Aktualizowane na bieżąco z rozwojem projektu* 📈