#include "bsp_buzzer.h"
#include "pico/time.h"
#include <stdio.h>

static uint slice_num;
static uint pwm_chan;
static bool buzzer_active = false;
static absolute_time_t beep_end_time;

void bsp_buzzer_init(void) {
    // Inicjalizuj GPIO jako output i wyłącz
    gpio_init(BUZZER_PIN);
    gpio_set_dir(BUZZER_PIN, GPIO_OUT);
    gpio_put(BUZZER_PIN, false);
    
    // Teraz ustaw PWM
    gpio_set_function(BUZZER_PIN, GPIO_FUNC_PWM);
    
    // Znajdź slice PWM dla tego pinu
    slice_num = pwm_gpio_to_slice_num(BUZZER_PIN);
    pwm_chan = pwm_gpio_to_channel(BUZZER_PIN);
    
    // Wyłącz PWM slice przed konfiguracją
    pwm_set_enabled(slice_num, false);
    
    // Konfiguruj PWM - wyłącz oba kanały
    pwm_set_wrap(slice_num, 1000);
    pwm_set_chan_level(slice_num, PWM_CHAN_A, 0);
    pwm_set_chan_level(slice_num, PWM_CHAN_B, 0);
    
    // Włącz PWM slice
    pwm_set_enabled(slice_num, true);
    
    buzzer_active = false;
    
    printf("[BUZZER] Initialized on GPIO %d (PWM slice %d, channel %d) - SILENT\n", BUZZER_PIN, slice_num, pwm_chan);
}

void bsp_buzzer_set_frequency(uint32_t freq_hz) {
    if (freq_hz == 0) {
        bsp_buzzer_stop();
        return;
    }
    
    // Oblicz wrap value dla żądanej częstotliwości
    // System clock = 125MHz, więc wrap = 125000000 / freq_hz
    uint32_t wrap = 125000000 / freq_hz;
    if (wrap > 65535) wrap = 65535; // Maksymalna wartość dla 16-bit counter
    if (wrap < 2) wrap = 2;         // Minimalna wartość
    
    pwm_set_wrap(slice_num, wrap);
    pwm_set_chan_level(slice_num, pwm_chan, wrap / 2); // 50% duty cycle
    
    buzzer_active = true;
}

void bsp_buzzer_start(uint32_t freq_hz) {
    printf("[BUZZER] Starting tone: %lu Hz\n", freq_hz);
    bsp_buzzer_set_frequency(freq_hz);
    printf("[BUZZER] Tone active: %s\n", buzzer_active ? "YES" : "NO");
}

void bsp_buzzer_stop(void) {
    pwm_set_chan_level(slice_num, pwm_chan, 0);
    buzzer_active = false;
    printf("[BUZZER] STOPPED - PWM disabled\n");
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

bool bsp_buzzer_is_active(void) {
    // Sprawdź czy beep się skończył
    if (buzzer_active && absolute_time_diff_us(beep_end_time, get_absolute_time()) <= 0) {
        bsp_buzzer_stop();
    }
    
    return buzzer_active;
}