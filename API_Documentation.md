# API Endpoints dla Termostatu Inteligentnego

## Architektura Komunikacji

### 1. Endpointy API Serwera

#### GET /device/{id}/settings
**Opis:** Pobiera aktualne ustawienia termostatu (wykonywane co 5 sekund)  
**Autoryzacja:** Brak wymaganej autoryzacji (endpoint publiczny)  
**Timeout:** 4s progress timeout + 1M iteracji emergency backup  
**Response:**
```json
{
  "target_temp_c": 22.5,
  "last_source": "app",
  "updated_at": "2025-12-22T10:30:00Z"
}
```

**Pola:**
- `target_temp_c` (float): Zadana temperatura w stopniach Celsjusza
- `last_source` (string): Źródło ostatniej zmiany ("app", "device")
- `updated_at` (string): Timestamp ostatniej aktualizacji

#### PUT /device/{id}/settings  
**Opis:** Ustawia nową temperaturę zadaną z termostatu [OPCJONALNE]  
**Autoryzacja:** Brak wymaganej autoryzacji  
**Mutex:** Chronione przez http_in_progress  
**Request Body:**
```json
{
  "target_temp_c": 23.0,
  "source": "device"
}
```

**Response:** `200 OK` lub kod błędu

#### POST /device/{id}/reading
**Opis:** Wysyła odczyty z sensorów i setpoint (co 30 sekund)  
**Autoryzacja:** Brak wymaganej autoryzacji  
**Mutex:** Chronione przez http_in_progress  
**Request Body:**
```json
{
  "temperature": 21.5,
  "humidity": 45.2, 
  "pressure": 1013.25,
  "target_temp_c": 22.0,
  "window_open": false,
  "timestamp": 1703123456,
  "device_id": 1
}
```

### 2. Logika Biznesowa

#### Źródła Zmian Temperatury
- `"app"` - zmiana pochodzi z aplikacji mobilnej
- `"device"` - zmiana pochodzi z termostatu lokalnie  
- `null` - nieznane źródło (stare dane)

#### Scenariusze Obsługi

**A) Startup Termostatu:**
1. Pobierz aktualne ustawienia z serwera (GET /settings)
2. Ustaw lokalną zadaną temperaturę
3. Wyświetl status połączenia
4. Uruchom cykliczne sprawdzanie serwera

**B) Użytkownik zmienia temperaturę lokalnie:**
1. Aktywuj ochronę lokalnej zmiany (5s)
2. Spróbuj bezpośredniego ustawienia (PUT /settings)
3. Jeśli PUT się nie udał, wyślij przez POST /reading
4. Pokaż komunikat o wysłaniu
5. Zaplanuj szybkie sprawdzenie serwera (2s)

**C) Cykliczne sprawdzanie serwera:**
1. Pobierz ustawienia z serwera (GET /settings)
2. Sprawdź `last_source`:
   - Jeśli `"app"` - zaktualizuj lokalną wartość i pokaż powiadomienie
   - Jeśli `"device"` - ignoruj (to nasza zmiana)
   - Jeśli różnica > 0.05°C - zaktualizuj i powiadom o zmianie zewnętrznej
3. Aktualizuj cache backendu

### 3. Obsługa Błędów

**Brak połączenia:**
- Kontynuuj z ostatnimi znanymi ustawieniami
- Wyświetl status "tryb offline"
- Zapisuj pending zmiany do wysłania po odzyskaniu połączenia

**Błąd HTTP:**
- Spróbuj ponownie po opóźnieniu
- Pokaż komunikat błędu użytkownikowi
- Przejdź do trybu offline po kilku próbach

**Nieprawidłowe dane:**
- Użyj wartości domyślnych
- Zaloguj błąd do debugowania
- Wyświetl ostrzeżenie użytkownikowi

### 4. Komunikaty dla Użytkownika

#### Standardowe
- "Połączono z serwerem"
- "Temperatura zaktualizowana" 
- "Wysłano do aplikacji"

#### Zmiany Zewnętrzne
- "Nowa temperatura z aplikacji: 22.5°C"
- "Ustawienia zmienione zdalnie"

#### Błędy
- "Brak połączenia z serwerem"
- "Nie udało się wysłać danych"
- "Błąd komunikacji - pracuję offline"

### 5. Parametry Konfiguracyjne

```c
// Interwały czasowe
SERVER_CHECK_INTERVAL_MS    = 8000    // Sprawdzanie serwera co 8s
READING_SEND_INTERVAL_MS    = 15000   // Wysyłanie odczytów co 15s  
LOCAL_OVERRIDE_WINDOW_MS    = 5000    // Okno ochrony lokalnej zmiany
CONNECTION_TIMEOUT_MS       = 4000    // Timeout HTTP
HEARTBEAT_INTERVAL_MS       = 30000   // Heartbeat co 30s

// Progi temperatur
TEMP_SYNC_THRESHOLD        = 0.05f   // Próg synchronizacji (°C)
DEFAULT_TARGET_TEMP        = 21.0f   // Domyślna temperatura (°C)
```

### 6. Przykłady Testowania

```bash
# Pobranie ustawień
curl -X GET http://192.168.55.119:8000/device/1/settings

# Ustawienie temperatury z urządzenia  
curl -X PUT http://192.168.55.119:8000/device/1/settings \
  -H "Content-Type: application/json" \
  -d '{"target_temp_c": 23.0, "source": "device"}'

# Wysłanie odczytu
curl -X POST http://192.168.55.119:8000/device/1/reading \
  -H "Content-Type: application/json" \
  -d '{
    "temperature_c": 21.5,
    "humidity_pct": 45.2, 
    "pressure_hpa": 1013.25,
    "setpoint_c": 22.0
  }'
```

### 7. Scenariusze Testowe

1. **Zmiana temperatury lokalnie** → sprawdź czy wysłane na serwer
2. **Zmiana w aplikacji** → sprawdź czy pobrane przez termostat  
3. **Brak internetu** → sprawdź czy termostat działa offline
4. **Powrót internetu** → sprawdź czy synchronizuje się z serwerem
5. **Błędne dane z serwera** → sprawdź obsługę błędów
6. **Jednoczesne zmiany** → sprawdź rozwiązywanie konfliktów
7. **Restart termostatu** → sprawdź startup sync
8. **Długi brak połączenia** → sprawdź retry logic
