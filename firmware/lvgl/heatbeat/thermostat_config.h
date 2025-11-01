// thermostat_config.h
// Konfiguracja termostatu inteligentnego HeatBeat-Pico

#ifndef THERMOSTAT_CONFIG_H
#define THERMOSTAT_CONFIG_H

// ==================== PARAMETRY KOMUNIKACJI ====================
// Interwał sprawdzania serwera (ms)
#ifndef SERVER_CHECK_INTERVAL_MS
#define SERVER_CHECK_INTERVAL_MS    8000
#endif

// Interwał wysyłania odczytów (ms)
#ifndef READING_SEND_INTERVAL_MS  
#define READING_SEND_INTERVAL_MS    15000
#endif

// Okno ochrony lokalnej zmiany (ms)
#ifndef LOCAL_OVERRIDE_WINDOW_MS
#define LOCAL_OVERRIDE_WINDOW_MS    5000
#endif

// Timeout dla połączeń HTTP (ms)
#ifndef CONNECTION_TIMEOUT_MS
#define CONNECTION_TIMEOUT_MS       4000
#endif

// Interwał heartbeat (ms)
#ifndef HEARTBEAT_INTERVAL_MS
#define HEARTBEAT_INTERVAL_MS       30000
#endif

// Maksymalny czas oczekiwania na WiFi podczas startu (ms)
#ifndef WIFI_CONNECT_TIMEOUT_MS
#define WIFI_CONNECT_TIMEOUT_MS     15000
#endif

// ==================== PARAMETRY TERMOSTATU ====================
// Domyślna temperatura zadana (°C)
#ifndef DEFAULT_TARGET_TEMP
#define DEFAULT_TARGET_TEMP         21.0f
#endif

// Minimalny próg różnicy temperatur do synchronizacji (°C)
#ifndef TEMP_SYNC_THRESHOLD
#define TEMP_SYNC_THRESHOLD         0.05f
#endif

// Zakres temperatur łuku (0.1°C steps: 100 = 10.0°C, 400 = 40.0°C)
#ifndef ARC_TEMP_MIN
#define ARC_TEMP_MIN                100
#endif

#ifndef ARC_TEMP_MAX
#define ARC_TEMP_MAX                400
#endif

// ==================== PARAMETRY UI ====================
// Czas wyświetlania powiadomień (ms)
#ifndef NOTIFICATION_DURATION_MS
#define NOTIFICATION_DURATION_MS    3000
#endif

// Czas wyświetlania ikony telefonu przy zmianie z aplikacji (ms)
#ifndef PHONE_ICON_DURATION_MS
#define PHONE_ICON_DURATION_MS      5000
#endif

// Interwał aktualizacji statusu WiFi (ms)
#ifndef WIFI_STATUS_UPDATE_MS
#define WIFI_STATUS_UPDATE_MS       15000
#endif

// ==================== PROGI RSSI WIFI ====================
#define WIFI_RSSI_EXCELLENT         -50
#define WIFI_RSSI_GOOD              -70

// ==================== KOMUNIKATY DLA UŻYTKOWNIKA ====================
#define MSG_CONNECTING_WIFI         "Łączenie z WiFi..."
#define MSG_CONNECTING_SERVER       "Łączenie z serwerem..."
#define MSG_CONNECTED_SERVER        "Połączono z serwerem"
#define MSG_WIFI_OFFLINE            "WiFi niedostępne - tryb offline"
#define MSG_CONNECTION_LOST         "Utracono połączenie"
#define MSG_NO_INTERNET             "Brak połączenia z internetem"
#define MSG_COMM_ERROR              "Błąd komunikacji z serwerem"
#define MSG_OFFLINE_MODE            "Błąd komunikacji - pracuję offline"
#define MSG_SEND_FAILED             "Nie udało się wysłać danych"

#define MSG_TEMP_UPDATED            "Temperatura zaktualizowana"
#define MSG_TEMP_CONFIRMED          "Temperatura potwierdzona"
#define MSG_SENDING                 "Wysyłanie..."
#define MSG_SENT_TO_APP             "Wysłano do aplikacji"
#define MSG_REMOTE_CHANGE           "Ustawienia zmienione zdalnie"

// Format dla zmian z aplikacji
#define MSG_APP_CHANGE_FMT          "Nowa temperatura z aplikacji: %.1f°C"

// ==================== PARAMETRY DEBUGOWANIA ====================
#ifndef DEBUG_VERBOSE
#define DEBUG_VERBOSE               1
#endif

#ifndef DEBUG_NETWORK
#define DEBUG_NETWORK               1
#endif

#ifndef DEBUG_SYNC
#define DEBUG_SYNC                  1
#endif

// ==================== MAKRA POMOCNICZE ====================
#if DEBUG_VERBOSE
    #define DEBUG_PRINT(fmt, ...) printf("[DEBUG] " fmt "\n", ##__VA_ARGS__)
#else
    #define DEBUG_PRINT(fmt, ...) ((void)0)
#endif

#if DEBUG_NETWORK
    #define NET_PRINT(fmt, ...) printf("[NET] " fmt "\n", ##__VA_ARGS__)
#else
    #define NET_PRINT(fmt, ...) ((void)0)
#endif

#if DEBUG_SYNC
    #define SYNC_PRINT(fmt, ...) printf("[SYNC] " fmt "\n", ##__VA_ARGS__)
#else
    #define SYNC_PRINT(fmt, ...) ((void)0)
#endif

#endif // THERMOSTAT_CONFIG_H