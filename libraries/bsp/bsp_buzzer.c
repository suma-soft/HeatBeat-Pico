#include "bsp_buzzer.h"
#include "pico/time.h"
#include <stdio.h>

static uint slice_num;
static uint pwm_chan;
static bool buzzer_active = false;
static absolute_time_t beep_end_time;

void bsp_buzzer_init(void) {
    printf("[BUZZER] Initializing buzzer on GPIO %d...\n", BUZZER_PIN);
    
    // RESET GPIO najpierw
    gpio_init(BUZZER_PIN);
    gpio_set_dir(BUZZER_PIN, GPIO_OUT);
    gpio_put(BUZZER_PIN, false);
    sleep_ms(10); // Krótka pauza
    
    // Ustaw PWM function
    gpio_set_function(BUZZER_PIN, GPIO_FUNC_PWM);
    
    // Znajdź PWM parameters
    slice_num = pwm_gpio_to_slice_num(BUZZER_PIN);
    pwm_chan = pwm_gpio_to_channel(BUZZER_PIN);
    
    printf("[BUZZER] PWM mapping: GPIO %d -> slice %d, channel %d\n", 
           BUZZER_PIN, slice_num, pwm_chan);
    
    // RESET PWM slice całkowicie
    pwm_set_enabled(slice_num, false);
    
    // Podstawowa konfiguracja PWM
    pwm_config config = pwm_get_default_config();
    pwm_config_set_clkdiv(&config, 125.0f); // 1MHz PWM clock (125MHz / 125)
    pwm_config_set_wrap(&config, 1000);     // 1kHz base frequency (1MHz / 1000)
    
    // Zastosuj konfigurację
    pwm_init(slice_num, &config, false); // false = nie uruchamiaj jeszcze
    
    // Wyzeruj oba kanały
    pwm_set_chan_level(slice_num, PWM_CHAN_A, 0);
    pwm_set_chan_level(slice_num, PWM_CHAN_B, 0);
    
    buzzer_active = false;
    
    printf("[BUZZER] Initialized - READY (slice %d disabled, channels zeroed)\n", slice_num);
}

void bsp_buzzer_set_frequency(uint32_t freq_hz) {
    if (freq_hz == 0) {
        bsp_buzzer_stop();
        return;
    }
    
    // POPRAWKA OBLICZANIA CZĘSTOTLIWOŚCI PWM
    // System clock = 125MHz, potrzebujemy dzielnik i wrap dla częstotliwości
    
    // Dla buzzera pasywnego potrzebujemy niższą częstotliwość PWM
    // Użyjmy divider aby uzyskać właściwą częstotliwość
    float divider = 125000000.0f / (freq_hz * 1000.0f); // 1000 to wrap value
    if (divider < 1.0f) divider = 1.0f;
    if (divider > 255.0f) divider = 255.0f;
    
    uint32_t wrap = 1000; // Stała wartość wrap
    
    printf("[BUZZER] Freq: %lu Hz, Divider: %.2f, Wrap: %lu\n", freq_hz, divider, wrap);
    
    // Ustaw dzielnik częstotliwości
    pwm_set_clkdiv(slice_num, divider);
    
    // Ustaw wrap i duty cycle
    pwm_set_wrap(slice_num, wrap);
    pwm_set_chan_level(slice_num, pwm_chan, wrap / 2); // 50% duty cycle
    
    // WŁĄCZ PWM SLICE ponownie po zmianie
    pwm_set_enabled(slice_num, true);
    
    buzzer_active = true;
    
    printf("[BUZZER] PWM configured: slice=%d, chan=%d, level=%lu\n", 
           slice_num, pwm_chan, wrap/2);
}

void bsp_buzzer_start(uint32_t freq_hz) {
    printf("[BUZZER] Starting tone: %lu Hz\n", freq_hz);
    bsp_buzzer_set_frequency(freq_hz);
    printf("[BUZZER] Tone active: %s\n", buzzer_active ? "YES" : "NO");
}

void bsp_buzzer_stop(void) {
    // Wyłącz level na właściwym kanale
    pwm_set_chan_level(slice_num, pwm_chan, 0);
    
    // DODATKOWE zabezpieczenie - wyłącz oba kanały
    pwm_set_chan_level(slice_num, PWM_CHAN_A, 0);
    pwm_set_chan_level(slice_num, PWM_CHAN_B, 0);
    
    // Wyłącz cały PWM slice
    pwm_set_enabled(slice_num, false);
    
    buzzer_active = false;
    printf("[BUZZER] STOPPED - PWM slice %d disabled, all channels zeroed\n", slice_num);
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