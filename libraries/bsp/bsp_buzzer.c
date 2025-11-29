#include "bsp_buzzer.h"
#include "pico/time.h"
#include <stdio.h>

static bool buzzer_active = false;
static bool tone_generating = false;
static uint32_t tone_frequency = 1000;

void bsp_buzzer_init(void) {
    printf("[BUZZER] SIMPLE Init GPIO %d\n", BUZZER_PIN);
    
    // NAJPROSTSZA konfiguracja
    gpio_init(BUZZER_PIN);
    gpio_set_dir(BUZZER_PIN, GPIO_OUT);
    gpio_put(BUZZER_PIN, false);
    
    buzzer_active = false;
    printf("[BUZZER] SIMPLE Ready on GPIO %d\n", BUZZER_PIN);
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
    printf("[BUZZER] START TONE %lu Hz on GPIO %d\n", freq_hz, BUZZER_PIN);
    tone_frequency = freq_hz;
    tone_generating = true;
    buzzer_active = true;
    printf("[BUZZER] Tone generator ACTIVE\n");
}

void bsp_buzzer_stop(void) {
    tone_generating = false;
    gpio_put(BUZZER_PIN, false);
    buzzer_active = false;
    printf("[BUZZER] Tone generator STOPPED, GPIO %d = LOW\n", BUZZER_PIN);
}

void bsp_buzzer_beep(uint32_t freq_hz, uint32_t duration_ms) {
    printf("[BUZZER] BLOCKING BEEP: %lu Hz for %lu ms\n", freq_hz, duration_ms);
    
    uint32_t period_us = 1000000 / freq_hz;  // Okres w mikrosekundach
    uint32_t half_period_us = period_us / 2;  // Półokres
    uint32_t total_cycles = (duration_ms * 1000) / period_us;  // Liczba cykli
    
    printf("[BUZZER] Period: %lu us, Half: %lu us, Cycles: %lu\n", 
           period_us, half_period_us, total_cycles);
    
    buzzer_active = true;
    
    // GENERATOR TONU - szybkie przełączanie GPIO
    for (uint32_t i = 0; i < total_cycles; i++) {
        gpio_put(BUZZER_PIN, true);   // HIGH
        sleep_us(half_period_us);     // Czekaj pół okresu
        gpio_put(BUZZER_PIN, false);  // LOW  
        sleep_us(half_period_us);     // Czekaj pół okresu
    }
    
    gpio_put(BUZZER_PIN, false);  // Upewnij się że jest OFF
    buzzer_active = false;
    printf("[BUZZER] BEEP completed\n");
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
    return buzzer_active || tone_generating;
}

// FUNKCJA TESTOWA - bezpośrednie sterowanie GPIO
void bsp_buzzer_test_manual(bool on) {
    printf("[BUZZER] MANUAL TEST: GPIO %d = %s\n", BUZZER_PIN, on ? "HIGH" : "LOW");
    gpio_put(BUZZER_PIN, on);
    if (on) {
        printf("[BUZZER] *** BUZZER SHOULD BE ON NOW ***\n");
    } else {
        printf("[BUZZER] --- Buzzer off ---\n");
    }
}

// KRÓTKI SMUTNY SYGNAŁ - prosty i skuteczny
void bsp_buzzer_play_sad_beep(void) {
    printf("[BUZZER] Krótki smutny sygnał\n");
    
    buzzer_active = true;
    
    // Smutny sygnał: niski ton opadający
    bsp_buzzer_beep(600, 300);   // Średni ton
    sleep_ms(100);
    bsp_buzzer_beep(400, 500);   // Niższy ton, dłużej - smutno
    
    buzzer_active = false;
    printf("[BUZZER] Smutny sygnał zakończony\n");
}

// FANFARY STARTOWE - triumfalne przy włączeniu urządzenia
void bsp_buzzer_play_startup_fanfare(void) {
    printf("[BUZZER] Fanfary startowe\n");
    
    buzzer_active = true;
    
    // Triumfalne fanfary: C-E-G-C (akord C-dur w górę)
    bsp_buzzer_beep(523, 300);   // C5
    sleep_ms(50);
    bsp_buzzer_beep(659, 300);   // E5 
    sleep_ms(50);
    bsp_buzzer_beep(784, 300);   // G5
    sleep_ms(50);
    bsp_buzzer_beep(1047, 500);  // C6 - triumfalnie dłużej
    
    buzzer_active = false;
    printf("[BUZZER] Fanfary startowe zakończone\n");
}

// TRZY CORAZ WYŻSZE DŹWIĘKI - przy osiągnięciu temperatury
void bsp_buzzer_play_temp_achieved(void) {
    printf("[BUZZER] Temperatura osiągnięta - trzy wyższe tony\n");
    
    buzzer_active = true;
    
    // Trzy coraz wyższe dźwięki: F-A-C
    bsp_buzzer_beep(698, 200);   // F5
    sleep_ms(100);
    bsp_buzzer_beep(880, 200);   // A5
    sleep_ms(100);
    bsp_buzzer_beep(1047, 400);  // C6 - najwyższy, dłużej
    
    buzzer_active = false;
    printf("[BUZZER] Sygnalizacja osiągnięcia temperatury zakończona\n");
}

// PIĘĆ CORAZ NIŻSZYCH DŹWIĘKÓW - przy aktywacji grzania
void bsp_buzzer_play_heating_on(void) {
    printf("[BUZZER] Aktywacja grzania - pięć niższych tonów\n");
    
    buzzer_active = true;
    
    // Pięć coraz niższych dźwięków: A-F-D-B-G
    bsp_buzzer_beep(880, 150);   // A5
    sleep_ms(50);
    bsp_buzzer_beep(698, 150);   // F5
    sleep_ms(50);
    bsp_buzzer_beep(587, 150);   // D5
    sleep_ms(50);
    bsp_buzzer_beep(494, 150);   // B4
    sleep_ms(50);
    bsp_buzzer_beep(392, 300);   // G4 - najniższy, dłużej
    
    buzzer_active = false;
    printf("[BUZZER] Sygnalizacja włączenia grzania zakończona\n");
}