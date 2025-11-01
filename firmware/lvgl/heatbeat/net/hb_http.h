// hb_http.h
// Minimalny klient TCP (lwIP raw, NO_SYS) do zapytań HTTP/1.1.
// Wersja z GET /device/{id}/settings (parsuje target_temp_c)
// oraz POST /device/{id}/reading (wysyła T/RH/P + setpoint).

#ifndef HB_HTTP_H
#define HB_HTTP_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    HB_HTTP_OK = 0,
    HB_HTTP_ERR_PARAM   = -1,
    HB_HTTP_ERR_IP      = -2,
    HB_HTTP_ERR_CONNECT = -3,
    HB_HTTP_ERR_SEND    = -4,
    HB_HTTP_ERR_RECV    = -5,
    HB_HTTP_ERR_TIMEOUT = -6,
    HB_HTTP_ERR_PARSE   = -7,
} hb_http_status_t;

// Struktura dla parsowania odpowiedzi ustawień
typedef struct {
    float target_temp_c;
    char last_source[16];  // "app", "device", lub pusty string dla null
} hb_settings_response_t;

// GET /device/{id}/settings → zwraca target_temp_c i last_source
hb_http_status_t hb_http_get_settings(
    const char *host,
    uint16_t port,
    int device_id,
    hb_settings_response_t *out_settings,
    uint32_t timeout_ms
);

// GET /device/{id}/settings → zwraca target_temp_c (wstecznie kompatybilne)
hb_http_status_t hb_http_get_settings_target_temp(
    const char *host,
    uint16_t port,
    int device_id,
    float *out_target_c,
    uint32_t timeout_ms
);

// PUT /device/{id}/settings → ustawia target_temp_c ze źródłem "device"
hb_http_status_t hb_http_set_settings_target_temp(
    const char *host,
    uint16_t port,
    int device_id,
    float target_temp_c,
    uint32_t timeout_ms
);

// POST /device/{id}/reading z polami: temperature_c, humidity_pct, pressure_hpa, setpoint_c
hb_http_status_t hb_http_post_reading(
    const char *host,
    uint16_t port,
    int device_id,
    float temperature_c,
    float humidity_pct,
    float pressure_hpa,
    float setpoint_c,
    uint32_t timeout_ms
);

#ifdef __cplusplus
}
#endif

#endif // HB_HTTP_H
