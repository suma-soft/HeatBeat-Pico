#ifndef BSP_BUZZER_H
#define BSP_BUZZER_H

#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// GPIO pin dla buzzera KY-006
#define BUZZER_PIN 20

// Predefiniowane częstotliwości dla różnych tonów
#define BUZZER_FREQ_LOW     200    // Niski ton
#define BUZZER_FREQ_MID     800    // Średni ton
#define BUZZER_FREQ_HIGH    2000   // Wysoki ton
#define BUZZER_FREQ_ALARM   1000   // Alarm
#define BUZZER_FREQ_BEEP    1500   // Krótki sygnał

// Typy sygnałów
typedef enum {
    BUZZER_TONE_OFF = 0,
    BUZZER_TONE_BEEP,       // Krótki beep
    BUZZER_TONE_ALARM,      // Alarm ciągły
    BUZZER_TONE_WARNING,    // Ostrzeżenie (pulsy)
    BUZZER_TONE_SUCCESS,    // Sukces (melodyjka)
    BUZZER_TONE_ERROR       // Błąd (szybkie pulsy)
} buzzer_tone_t;

// Funkcje publiczne
void bsp_buzzer_init(void);
void bsp_buzzer_set_frequency(uint32_t freq_hz);
void bsp_buzzer_start(uint32_t freq_hz);
void bsp_buzzer_stop(void);
void bsp_buzzer_beep(uint32_t freq_hz, uint32_t duration_ms);
void bsp_buzzer_beep_async(uint32_t on_ms, uint32_t off_ms);  // Dla powtarzających się sygnałów
void bsp_buzzer_play_tone(buzzer_tone_t tone);
bool bsp_buzzer_is_active(void);

#ifdef __cplusplus
}
#endif

#endif // BSP_BUZZER_H