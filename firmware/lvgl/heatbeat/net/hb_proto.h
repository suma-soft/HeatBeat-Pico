// hb_proto.h
// Prosty moduł do budowania zapytań HTTP (POST/GET) oraz
// minimalnego parsowania JSON dla backendu HeatBeat-Control.
// Krok 1: tylko formatowanie danych i parsing — bez warstwy sieci.

#ifndef HB_PROTO_H
#define HB_PROTO_H

// ── nagłówki i portowalność C/C++ ────────────────────────────────────────────
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stddef.h>   // size_t

#if defined(__cplusplus)
  #include <cstdint>
  using std::uint16_t;
#else
  #include <stdint.h>
#endif

// ========================= KONFIGURACJA DOMYŚLNA ============================
// Możesz nadpisać te define'y przed #include "net/hb_proto.h"
#ifndef HB_DEFAULT_HOST
#define HB_DEFAULT_HOST "192.168.55.117"
#endif

#ifndef HB_DEFAULT_PORT
#define HB_DEFAULT_PORT 8000
#endif

#ifndef HB_DEFAULT_DEVICE_ID
#define HB_DEFAULT_DEVICE_ID 1
#endif

// ====================== BUDOWANIE TREŚCI JSON (POST) =========================
// Zwraca liczbę znaków lub <0 przy błędzie.
static inline int hb_build_reading_json(char *out, size_t cap,
                                        float temperature_c,
                                        float humidity_pct,
                                        float pressure_hpa,
                                        float setpoint_c,
                                        bool window_open_detected,
                                        bool is_heating)
{
    if (!out || cap == 0) return -1;

    int n = snprintf(out, cap,
                     "{\"temperature_c\":%.2f,"
                     "\"humidity_pct\":%.2f,"
                     "\"pressure_hpa\":%.2f,"
                     "\"setpoint_c\":%.2f,"
                     "\"window_open_detected\":%s,"
                     "\"is_heating\":%s}",
                     temperature_c, humidity_pct, pressure_hpa, setpoint_c,
                     window_open_detected ? "true" : "false",
                     is_heating ? "true" : "false");
    return (n >= 0 && (size_t)n < cap) ? n : -2;
}

// ==================== BUDOWANIE NAGŁÓWKÓW HTTP/1.1 (POST) ===================
// POST /device/{device_id}/reading
static inline int hb_build_http_post_reading(char *out, size_t cap,
                                             const char *host,
                                             uint16_t port,
                                             int device_id,
                                             const char *json_body)
{
    if (!out || cap == 0 || !host || !json_body) return -1;

    size_t body_len = strlen(json_body);
    int n = snprintf(out, cap,
        "POST /device/%d/reading HTTP/1.1\r\n"
        "Host: %s:%u\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: %u\r\n"
        "Connection: close\r\n"
        "\r\n"
        "%s",
        device_id, host, (unsigned)port, (unsigned)body_len, json_body);

    return (n >= 0 && (size_t)n < cap) ? n : -2;
}

// ===================== BUDOWANIE NAGŁÓWKÓW HTTP/1.1 (GET) ===================
// GET /device/{device_id}/settings
static inline int hb_build_http_get_settings(char *out, size_t cap,
                                             const char *host,
                                             uint16_t port,
                                             int device_id)
{
    if (!out || cap == 0 || !host) return -1;

    int n = snprintf(out, cap,
        "GET /device/%d/settings HTTP/1.1\r\n"
        "Host: %s:%u\r\n"
        "Accept: application/json\r\n"
        "Connection: close\r\n"
        "\r\n",
        device_id, host, (unsigned)port);

    return (n >= 0 && (size_t)n < cap) ? n : -2;
}

// ==================== BUDOWANIE NAGŁÓWKÓW HTTP/1.1 (PUT) ====================
// PUT /device/{device_id}/settings - do ustawiania temperatury zadanej
static inline int hb_build_http_put_settings(char *out, size_t cap,
                                             const char *host,
                                             uint16_t port,
                                             int device_id,
                                             const char *json_body)
{
    if (!out || cap == 0 || !host || !json_body) return -1;

    size_t body_len = strlen(json_body);
    int n = snprintf(out, cap,
        "PUT /device/%d/settings HTTP/1.1\r\n"
        "Host: %s:%u\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: %u\r\n"
        "Connection: close\r\n"
        "\r\n"
        "%s",
        device_id, host, (unsigned)port, (unsigned)body_len, json_body);

    return (n >= 0 && (size_t)n < cap) ? n : -2;
}

// ====================== BUDOWANIE JSON DLA USTAWIEŃ =========================
static inline int hb_build_settings_json(char *out, size_t cap,
                                         float target_temp_c)
{
    if (!out || cap == 0) return -1;

    int n = snprintf(out, cap,
                     "{\"target_temp_c\":%.2f,\"source\":\"device\"}",
                     target_temp_c);
    return (n >= 0 && (size_t)n < cap) ? n : -2;
}

// ======================= PARSOWANIE JSON (odpowiedź GET) =====================
// Szuka "target_temp_c": <float>
static inline bool hb_parse_target_temp_from_json(const char *json,
                                                  float *out_target_c)
{
    if (!json || !out_target_c) return false;

    const char *key = "\"target_temp_c\"";
    const char *p = strstr(json, key);
    if (!p) return false;

    p = strchr(p + strlen(key), ':');
    if (!p) return false;
    p++;

    while (*p == ' ' || *p == '\t') p++;

    float tmp = 0.0f;
    if (sscanf(p, "%f", &tmp) != 1) return false;

    *out_target_c = tmp;
    return true;
}

// Parsuje "last_source": "string" z JSON
static inline bool hb_parse_last_source_from_json(const char *json,
                                                  char *out_source,
                                                  size_t source_cap)
{
    if (!json || !out_source || source_cap == 0) return false;

    const char *key = "\"last_source\"";
    const char *p = strstr(json, key);
    if (!p) {
        out_source[0] = '\0';
        return true; // source może być null/brak
    }

    p = strchr(p + strlen(key), ':');
    if (!p) return false;
    p++;

    // Pomiń spacje
    while (*p == ' ' || *p == '\t') p++;

    // Sprawdź czy null
    if (strncmp(p, "null", 4) == 0) {
        out_source[0] = '\0';
        return true;
    }

    // Szukaj cudzysłowu
    if (*p != '"') return false;
    p++;

    // Kopiuj do zamykającego cudzysłowu
    size_t i = 0;
    while (*p && *p != '"' && i < source_cap - 1) {
        out_source[i++] = *p++;
    }
    out_source[i] = '\0';

    return (*p == '"');
}

// ============================ POMOCNICZE PRESETY =============================
static inline int hb_make_post_reading_default(char *out, size_t cap,
                                               float t_c, float rh_pct,
                                               float p_hpa, float set_c,
                                               bool window_open_detected,
                                               bool is_heating)
{
    char json[256];
    int jn = hb_build_reading_json(json, sizeof(json), t_c, rh_pct, p_hpa, set_c, window_open_detected, is_heating);
    if (jn < 0) return jn;

    return hb_build_http_post_reading(out, cap,
                                      HB_DEFAULT_HOST, HB_DEFAULT_PORT,
                                      HB_DEFAULT_DEVICE_ID, json);
}

static inline int hb_make_get_settings_default(char *out, size_t cap)
{
    return hb_build_http_get_settings(out, cap,
                                      HB_DEFAULT_HOST, HB_DEFAULT_PORT,
                                      HB_DEFAULT_DEVICE_ID);
}

#endif // HB_PROTO_H
