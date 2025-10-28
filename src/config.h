#ifndef CONFIG_H
#define CONFIG_H

// Konfiguracja — dostosuj przed kompilacją lub wstaw mechanizm odczytu z Flash/FS
#define WIFI_SSID "YOUR_SSID"
#define WIFI_PASS "YOUR_PASSWORD"

// Adres backendu HeatBeat-Control (bez http://), np. "192.168.1.100" lub "myhost.local"
#define SERVER_HOST "192.168.1.100"
// Port backendu (domyślnie uvicorn 8000 lub 80)
#define SERVER_PORT 8000

// Id termostatu (utworzony w backendzie dla użytkownika)
#define THERMOSTAT_ID 1

// Interwał wysyłania (ms)
#define LOOP_DELAY_MS (60 * 1000)

#endif // CONFIG_H