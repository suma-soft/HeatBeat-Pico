#include "bsp_buzzer.h"
#include "pico/time.h"
#include <stdio.h>

static bool buzzer_active = false;
static absolute_time_t beep_end_time;

void bsp_buzzer_init(void) {
    printf("[BUZZER] Init GPIO %d (simple toggle mode)\n", BUZZER_PIN);
    
    // PROSTA konfiguracja - tylko GPIO output
    gpio_init(BUZZER_PIN);
    gpio_set_dir(BUZZER_PIN, GPIO_OUT);
    gpio_put(BUZZER_PIN, false);
    
    buzzer_active = false;
    printf("[BUZZER] Ready - GPIO mode\n");
}

void bsp_buzzer_set_frequency(uint32_t freq_hz) {
    if (freq_hz == 0) {
        bsp_buzzer_stop();
        return;
    }
    
    printf("[BUZZER] Simple frequency: %lu Hz (GPIO toggle)\n", freq_hz);
    // W tej wersji ignorujemy częstotliwość, po prostu włączamy GPIO
    gpio_put(BUZZER_PIN, true);
    buzzer_active = true;
}

void bsp_buzzer_start(uint32_t freq_hz) {
    printf("[BUZZER] Starting simple tone: %lu Hz\n", freq_hz);
    // Prosta implementacja - tylko włącz GPIO na HIGH
    gpio_put(BUZZER_PIN, true);
    buzzer_active = true;
    printf("[BUZZER] GPIO ON\n");
}

void bsp_buzzer_stop(void) {
    gpio_put(BUZZER_PIN, false);
    buzzer_active = false;
    printf("[BUZZER] GPIO OFF\n");
}

void bsp_buzzer_beep(uint32_t freq_hz, uint32_t duration_ms) {
    bsp_buzzer_start(freq_hz);
    beep_end_time = make_timeout_time_ms(duration_ms);
    printf("[BUZZER] Beep: %lu Hz for %lu ms\n", freq_hz, duration_ms);
}

void bsp_buzzer_play_tone(buzzer_tone_t tone) {
    switch (tone) {
        case BUZZER_TONE_BEEP:
            bsp_buzzer_beep(BUZZER_FREQ_BEEP, 200); // 200ms beep
            break;
            
        case BUZZER_TONE_ALARM:
            bsp_buzzer_start(BUZZER_FREQ_ALARM);
            break;
            
        case BUZZER_TONE_WARNING:
            // Implementacja pulsów - można rozszerzyć o timer
            bsp_buzzer_beep(BUZZER_FREQ_MID, 500);
            break;
            
        case BUZZER_TONE_SUCCESS:
            // Krótka melodyjka sukcesu
            bsp_buzzer_beep(BUZZER_FREQ_MID, 100);
            break;
            
        case BUZZER_TONE_ERROR:
            // Szybkie błędne pulsy
            bsp_buzzer_beep(BUZZER_FREQ_HIGH, 100);
            break;
            
        case BUZZER_TONE_OFF:
        default:
            bsp_buzzer_stop();
            break;
    }
}

void bsp_buzzer_beep_async(uint32_t on_ms, uint32_t off_ms) {
    // Prosta implementacja - w przyszłości można rozszerzyć o timer callback
    bsp_buzzer_beep(BUZZER_FREQ_ALARM, on_ms);
    // off_ms będzie obsługiwane przez główną pętlę
}

bool bsp_buzzer_is_active(void) {
    // Sprawdź czy beep się skończył
    if (buzzer_active && absolute_time_diff_us(beep_end_time, get_absolute_time()) <= 0) {
        bsp_buzzer_stop();
    }
    
    return buzzer_active;
}