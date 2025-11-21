extern void heatbeat_on_target_temp_changed(float new_target);
#include "main_screen.h"
#include <stdio.h>
#include "lv_font_montserrat_28_pl.h"
#include "lvgl.h"

// ─────────────────────────────
// GLOBALNE DANE
// ─────────────────────────────
float current_temp = 0;
int humidity = 0;
float pressure = 0;

lv_obj_t *ui_main_screen;
lv_obj_t *label_set_temp;
lv_obj_t *label_time;
lv_obj_t *label_pres;
lv_obj_t *label_temp;
lv_obj_t *label_humi;
lv_obj_t *label_target;
lv_obj_t *label_status;        // Status komunikacji
lv_obj_t *status_dot;          // Kropka statusu dla zablokowanego ekranu
lv_obj_t *label_notification; // Powiadomienia
lv_obj_t *icon_wifi;          // Ikona WiFi
lv_obj_t *icon_phone;         // Ikona telefonu
lv_obj_t *arc_control;        // referencja do łuku

float set_temperature = 21.0f;
static int target_temp = 22;

// Timer dla powiadomień
static uint32_t notification_hide_time = 0;

// Status połączenia
static bool connection_status = false; // false = brak połączenia, true = połączono

// Blokada ekranu
static bool screen_locked = true;  // Domyślnie zablokowany
static int tap_count = 0;
static uint32_t last_tap_time = 0;
static const uint32_t TAP_TIMEOUT_MS = 1000; // 1000ms na 3 dotknięcia (więcej czasu)
static const uint32_t AUTO_LOCK_MS = 60000; // 60s auto-lock (1 minuta)
static uint32_t last_activity_time = 0;

// ─────────────────────────────
// FUNKCJE BLOKADY EKRANU
// ─────────────────────────────
void update_screen_locked_state(void) {
    printf("[UI] update_screen_locked_state() called, screen_locked=%d\n", screen_locked ? 1 : 0);
    
    if (screen_locked) {
        printf("[UI] Setting LOCKED state\n");
        // Ekran zablokowany - czarne tło, ciemno szary tekst, ukryj suwak i status
        lv_obj_set_style_bg_color(ui_main_screen, lv_color_black(), LV_PART_MAIN);
        lv_obj_add_flag(arc_control, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(arc_control, LV_OBJ_FLAG_CLICKABLE); // Wyłącz klikanie arc w trybie locked
        lv_obj_add_flag(label_status, LV_OBJ_FLAG_HIDDEN); // Ukryj status tekstowy
        
        // Pokaż symbol statusu
        if (status_dot) {
            lv_obj_clear_flag(status_dot, LV_OBJ_FLAG_HIDDEN);
            // Ustaw symbol i kolor na podstawie statusu połączenia
            if (connection_status) {
                lv_label_set_text(status_dot, "@"); // @ dla połączenia
                lv_obj_set_style_text_color(status_dot, lv_color_make(0, 100, 0), LV_PART_MAIN); // Ciemny zielony
            } else {
                lv_label_set_text(status_dot, "X"); // X dla braku połączenia
                lv_obj_set_style_text_color(status_dot, lv_color_make(100, 0, 0), LV_PART_MAIN); // Ciemny czerwony
            }
        }
        
        // Ciemno szary kolor tekstu dla temperatury
        lv_obj_set_style_text_color(label_temp, lv_color_make(80, 80, 80), LV_PART_MAIN);
        lv_obj_set_style_text_color(label_set_temp, lv_color_make(80, 80, 80), LV_PART_MAIN);
        lv_obj_set_style_text_color(label_humi, lv_color_make(60, 60, 60), LV_PART_MAIN);
        lv_obj_set_style_text_color(label_pres, lv_color_make(60, 60, 60), LV_PART_MAIN);
        
        // NIE pokazuj komunikatu od razu - pokaże się po pierwszym dotknięciu
    } else {
        printf("[UI] Setting UNLOCKED state\n");
        // Ekran odblokowany - czarne tło, białe napisy, pokaż suwak i status
        lv_obj_set_style_bg_color(ui_main_screen, lv_color_black(), LV_PART_MAIN);
        lv_obj_clear_flag(arc_control, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(arc_control, LV_OBJ_FLAG_CLICKABLE); // Włącz klikanie arc w trybie unlocked
        lv_obj_clear_flag(label_status, LV_OBJ_FLAG_HIDDEN); // Pokaż status tekstowy
        
        // Ukryj symbol statusu
        if (status_dot) {
            lv_obj_add_flag(status_dot, LV_OBJ_FLAG_HIDDEN);
        }
        
        // Białe kolory tekstu
        lv_obj_set_style_text_color(label_temp, lv_color_white(), LV_PART_MAIN);
        lv_obj_set_style_text_color(label_set_temp, lv_color_white(), LV_PART_MAIN);
        lv_obj_set_style_text_color(label_humi, lv_color_white(), LV_PART_MAIN);
        lv_obj_set_style_text_color(label_pres, lv_color_white(), LV_PART_MAIN);
        lv_obj_set_style_text_color(label_status, lv_color_make(200, 200, 200), LV_PART_MAIN);
        
        // Ukryj komunikat o odblokowaniu
        if (label_notification) {
            lv_obj_add_flag(label_notification, LV_OBJ_FLAG_HIDDEN);
            notification_hide_time = 0; // Resetuj timer
        }
    }
    printf("[UI] update_screen_locked_state() finished\n");
}

void handle_screen_tap(void) {
    uint32_t now = lv_tick_get();
    printf("[UI] Screen tap detected, locked=%d\n", screen_locked ? 1 : 0);
    
    if (screen_locked) {
        // Natychmiast ukryj poprzedni komunikat przy nowym dotknięciu
        if (label_notification) {
            lv_obj_add_flag(label_notification, LV_OBJ_FLAG_HIDDEN);
            notification_hide_time = 0;
        }
        
        // Sprawdź czy to jest w czasie na potrójne dotknięcie
        if (now - last_tap_time < TAP_TIMEOUT_MS) {
            tap_count++;
            printf("[UI] Tap count increased to %d\n", tap_count);
        } else {
            tap_count = 1; // Reset licznika
            printf("[UI] Tap count reset to 1\n");
        }
        
        last_tap_time = now;
        
        // Pokaż feedback z odliczaniem pozostałych dotknięć
        char tap_feedback[48];
        int remaining = 3 - tap_count;
        if (remaining > 0) {
            if (remaining == 1) {
                snprintf(tap_feedback, sizeof(tap_feedback), "Ostatnie dotknięcie\naby odblokować");
            } else {
                snprintf(tap_feedback, sizeof(tap_feedback), "%d dotknięcia pozostały", remaining);
            }
        } else {
            snprintf(tap_feedback, sizeof(tap_feedback), "Odblokowywanie...");
        }
        main_screen_show_notification(tap_feedback, 3000); // 3 sekundy na każdy komunikat
        
        if (tap_count >= 3) {
            // Odblokuj ekran
            printf("[UI] Before unlock: screen_locked=%d, tap_count=%d\n", screen_locked ? 1 : 0, tap_count);
            screen_locked = false;
            tap_count = 0;
            
            // WAŻNE: Reset activity timer przy odblokowaniu!
            last_activity_time = now;
            printf("[UI] Activity timer reset on unlock\n");
            
            printf("[UI] After setting: screen_locked=%d, tap_count=%d\n", screen_locked ? 1 : 0, tap_count);
            printf("[UI] Ekran odblokowany\n");
            
            printf("[UI] Calling update_screen_locked_state()...\n");
            update_screen_locked_state();
            printf("[UI] update_screen_locked_state() completed\n");
            
            main_screen_show_notification("Ekran odblokowany!", 3000); // 3 sekundy widoczności
            printf("[UI] Unlock notification shown\n");
        }
    } else {
        // Ekran odblokowany - resetuj timer auto-lock
        last_activity_time = now;
        printf("[UI] Activity timer reset\n");
    }
}

void check_auto_lock(void) {
    if (!screen_locked) {
        uint32_t now = lv_tick_get();
        uint32_t time_since_activity = now - last_activity_time;
        
        // Debug tylko jeśli zbliżamy się do auto-lock (ostatnie 5 sekund)
        if (time_since_activity > (AUTO_LOCK_MS - 5000)) {
            printf("[UI] Auto-lock check: time_since_activity=%lu, threshold=%lu\n", 
                   time_since_activity, (unsigned long)AUTO_LOCK_MS);
        }
        
        if (time_since_activity > AUTO_LOCK_MS) {
            printf("[UI] Auto-lock triggered! time_since_activity=%lu > %lu\n", 
                   time_since_activity, (unsigned long)AUTO_LOCK_MS);
            screen_locked = true;
            printf("[UI] Auto-lock ekranu\n");
            update_screen_locked_state();
        }
    }
}

// ─────────────────────────────
// FUNKCJE POMOCNICZE
// ─────────────────────────────
void update_labels()
{
    char buf[32];

    if (screen_locked) {
        // Ekran zablokowany - tylko temperatura z BME (większa czcionka)
        snprintf(buf, sizeof(buf), "%.1f°C", current_temp);
        if (label_temp) lv_label_set_text(label_temp, buf);
        
        // Ukryj inne dane czujników w trybie zablokowanym
        if (label_humi) lv_obj_add_flag(label_humi, LV_OBJ_FLAG_HIDDEN);
        if (label_pres) lv_obj_add_flag(label_pres, LV_OBJ_FLAG_HIDDEN);
    } else {
        // Ekran odblokowany - wszystkie dane
        snprintf(buf, sizeof(buf), "Temperatura: %.1f°C", current_temp);
        if (label_temp) lv_label_set_text(label_temp, buf);

        snprintf(buf, sizeof(buf), "Wilgotność: %d%%", humidity);
        if (label_humi) {
            lv_label_set_text(label_humi, buf);
            lv_obj_clear_flag(label_humi, LV_OBJ_FLAG_HIDDEN);
        }

        float pressure_hpa = pressure * 0.01f;
        snprintf(buf, sizeof(buf), "Ciśnienie: %.2f hPa", pressure_hpa);
        if (label_pres) {
            lv_label_set_text(label_pres, buf);
            lv_obj_clear_flag(label_pres, LV_OBJ_FLAG_HIDDEN);
        }
    }

    snprintf(buf, sizeof(buf), "Zadana: %d°C", target_temp);
    if (label_target) lv_label_set_text(label_target, buf);
}

void update_set_temp_label(void) {
    char buf[32];
    snprintf(buf, sizeof(buf), "Zadana: %.1f°C", set_temperature);
    if (label_set_temp) {
        lv_label_set_text(label_set_temp, buf);
    }
}

// Interpolacja kolorów dla łuku
static lv_color_t interpolate_rgb(int r1, int g1, int b1, int r2, int g2, int b2, float ratio)
{
    int r = r1 + (int)((r2 - r1) * ratio);
    int g = g1 + (int)((g2 - g1) * ratio);
    int b = b1 + (int)((b2 - b1) * ratio);
    return lv_color_make(r, g, b);
}

static lv_color_t interpolate_color(float temp)
{
    if (temp <= 17.0f)
        return lv_color_make(100, 200, 255);
    if (temp <= 21.0f)
        return interpolate_rgb(100, 200, 255, 100, 255, 100, (temp - 17.0f) / 4.0f);
    if (temp <= 24.0f)
        return interpolate_rgb(100, 255, 100, 255, 255, 100, (temp - 21.0f) / 3.0f);
    if (temp <= 27.0f)
        return interpolate_rgb(255, 255, 100, 200, 80, 60, (temp - 24.0f) / 3.0f);
    if (temp < 30.0f)
        return interpolate_rgb(200, 80, 60, 255, 50, 50, (temp - 27.0f) / 3.0f);

    return lv_color_make(255, 50, 50);
}

static void update_arc_color(lv_obj_t *arc, float temperature)
{
    if (!arc) return;

    lv_color_t color = interpolate_color(temperature);
    lv_obj_set_style_arc_color(arc, color, LV_PART_MAIN);
    lv_obj_set_style_arc_color(arc, lv_color_make(40, 40, 40), LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(arc, color, LV_PART_KNOB);
    lv_obj_set_style_bg_color(arc, color, LV_PART_KNOB);
    lv_obj_set_style_bg_opa(arc, LV_OPA_COVER, LV_PART_KNOB);
    lv_obj_set_style_arc_opa(arc, LV_OPA_COVER, LV_PART_MAIN);
}

// ─────────────────────────────
// ZDARZENIA UI
// ─────────────────────────────
void screen_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    printf("[UI] Screen event: %d\n", code);
    if (code == LV_EVENT_PRESSED) {
        printf("[UI] Screen pressed event detected\n");
        handle_screen_tap();
    }
}

void arc_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *arc = lv_event_get_target(e);
    if (code == LV_EVENT_VALUE_CHANGED)
    {
        if (screen_locked) {
            // Zablokowany - przywróć poprzednią wartość
            lv_arc_set_value(arc, (int)(set_temperature * 10));
            return;
        }
        
        handle_screen_tap(); // Odśwież timer aktywności
        
        int val = lv_arc_get_value(arc);
        int reversed_val = 100 + 400 - val;
        set_temperature = reversed_val / 10.0f;
        update_set_temp_label();
        update_arc_color(arc, set_temperature);

        // 🔴 KLUCZOWE: zgłoś zmianę do logiki sieciowej (anty-nadpisywanie GET)
        heatbeat_on_target_temp_changed(set_temperature);
    }
}

static void slider_target_temp_event_cb(lv_event_t* e) {
    lv_obj_t* slider = lv_event_get_target(e);
    int32_t value = lv_slider_get_value(slider);
    float new_target = (float)value;

    char buf[16];
    snprintf(buf, sizeof(buf), "%.1f°C", new_target);
    lv_label_set_text(label_set_temp, buf);

    heatbeat_on_target_temp_changed(new_target);
}

// ─────────────────────────────
// FUNKCJE STATUSU I POWIADOMIEŃ
// ─────────────────────────────
void main_screen_show_status(const char *message, bool is_error) {
    // Aktualizuj status połączenia na podstawie komunikatu
    if (strstr(message, "Połączono") || strstr(message, "połączono") || strstr(message, "Polaczono") || strstr(message, "polaczono")) {
        connection_status = true;
    } else if (strstr(message, "Brak") || strstr(message, "Błąd") || strstr(message, "Blad") || strstr(message, "blad") || strstr(message, "brak") || 
               strstr(message, "Łączenie") || strstr(message, "Lączenie") || strstr(message, "lączenie") || strstr(message, "laczenie")) {
        connection_status = false; // Łączenie też traktujemy jako brak połączenia (czerwona kropka)
    }
    
    if (label_status) {
        lv_label_set_text(label_status, message);
        lv_obj_set_style_text_color(label_status, 
            is_error ? lv_color_make(255, 100, 100) : lv_color_make(100, 255, 100), 
            LV_PART_MAIN);
        
        // Pokaż status tekstowy tylko gdy ekran jest odblokowany
        if (!screen_locked) {
            lv_obj_clear_flag(label_status, LV_OBJ_FLAG_HIDDEN);
        }
    }
    
    // Aktualizuj symbol na zablokowanym ekranie
    if (status_dot) {
        if (screen_locked) {
            // Pokaż symbol z odpowiednim kolorem i znakiem
            lv_obj_clear_flag(status_dot, LV_OBJ_FLAG_HIDDEN);
            if (connection_status) {
                lv_label_set_text(status_dot, "@"); // @ dla połączenia
                lv_obj_set_style_text_color(status_dot, lv_color_make(0, 100, 0), LV_PART_MAIN); // Ciemny zielony
            } else {
                lv_label_set_text(status_dot, "X"); // X dla braku połączenia
                lv_obj_set_style_text_color(status_dot, lv_color_make(100, 0, 0), LV_PART_MAIN); // Ciemny czerwony
            }
        } else {
            // Ukryj symbol gdy ekran odblokowany
            lv_obj_add_flag(status_dot, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

void main_screen_set_notification_time(uint32_t time) {
    notification_hide_time = time;
}

void main_screen_show_notification(const char *message, int duration_ms) {
    if (label_notification) {
        lv_label_set_text(label_notification, message);
        lv_obj_clear_flag(label_notification, LV_OBJ_FLAG_HIDDEN);
        
        if (duration_ms == 0) {
            // 0 oznacza: nie ukrywaj automatycznie
            notification_hide_time = 0;
        } else {
            // Ustaw czas ukrycia na podstawie duration_ms
            notification_hide_time = lv_tick_get() + duration_ms;
        }
    }
}

void main_screen_update_wifi_status(bool connected, int rssi) {
    if (icon_wifi) {
        if (connected) {
            // Kolor zależny od siły sygnału
            lv_color_t color;
            if (rssi > -50) color = lv_color_make(100, 255, 100);      // Bardzo dobry
            else if (rssi > -70) color = lv_color_make(255, 255, 100); // Dobry
            else color = lv_color_make(255, 150, 100);                 // Słaby
            
            lv_obj_set_style_text_color(icon_wifi, color, LV_PART_MAIN);
            lv_label_set_text(icon_wifi, LV_SYMBOL_WIFI);
            lv_obj_clear_flag(icon_wifi, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_set_style_text_color(icon_wifi, lv_color_make(255, 100, 100), LV_PART_MAIN);
            lv_label_set_text(icon_wifi, LV_SYMBOL_CLOSE);
            lv_obj_clear_flag(icon_wifi, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

void main_screen_show_external_change(bool from_app) {
    if (icon_phone) {
        if (from_app) {
            lv_obj_set_style_text_color(icon_phone, lv_color_make(100, 150, 255), LV_PART_MAIN);
            lv_label_set_text(icon_phone, LV_SYMBOL_CALL);
            lv_obj_clear_flag(icon_phone, LV_OBJ_FLAG_HIDDEN);
            
            // Automatycznie ukryj po 5 sekundach
            // Implementacja będzie w main loop poprzez main_screen_update_timers_with_time()
        } else {
            lv_obj_add_flag(icon_phone, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

// Funkcja do wywoływania w pętli głównej - obsługa timerów UI
void main_screen_update_timers(void) {
    // Ta funkcja jest wywoływana z main.c z przekazanym czasem
}

void main_screen_update_timers_with_time(uint32_t now) {
    // Ukryj powiadomienie po czasie
    if (notification_hide_time > 0 && now >= notification_hide_time) {
        if (label_notification) {
            lv_obj_add_flag(label_notification, LV_OBJ_FLAG_HIDDEN);
        }
        notification_hide_time = 0;
    }
}

// ─────────────────────────────
// INICJALIZACJA EKRANU
// ─────────────────────────────
void main_screen_init(void)
{
    ui_main_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(ui_main_screen, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_text_font(ui_main_screen, &lv_font_montserrat_28_pl, 0);
    lv_scr_load(ui_main_screen);

    label_time = lv_label_create(ui_main_screen);

    // --- STATUS POŁĄCZENIA NA GÓRZE ---
    label_status = lv_label_create(ui_main_screen);
    lv_obj_set_style_text_color(label_status, lv_color_make(200, 200, 200), LV_PART_MAIN);
    lv_obj_align(label_status, LV_ALIGN_TOP_MID, 0, 40); // Wyżej, żeby nie zachodzić na temperaturę
    lv_label_set_text(label_status, "Laczenie...");

    // --- CZUJNIKI PONIŻEJ ---
    label_temp = lv_label_create(ui_main_screen);
    lv_obj_set_style_text_color(label_temp, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(label_temp, &lv_font_montserrat_28_pl, 0); // Większa czcionka
    lv_obj_align(label_temp, LV_ALIGN_TOP_MID, 0, 80); // Przesunięte z 60 na 80

    label_humi = lv_label_create(ui_main_screen);
    lv_obj_set_style_text_color(label_humi, lv_color_white(), LV_PART_MAIN);
    lv_obj_align(label_humi, LV_ALIGN_TOP_MID, 0, 130); // Przesunięte z 110 na 130

    label_pres = lv_label_create(ui_main_screen);
    lv_obj_set_style_text_color(label_pres, lv_color_white(), LV_PART_MAIN);
    lv_obj_align(label_pres, LV_ALIGN_TOP_MID, 0, 180); // Przesunięte z 160 na 180

    // --- POWIADOMIENIA ---
    label_notification = lv_label_create(ui_main_screen);
    lv_obj_set_style_text_color(label_notification, lv_color_make(255, 255, 100), LV_PART_MAIN);
    lv_obj_align(label_notification, LV_ALIGN_CENTER, 0, 80); // Wyżej - poniżej środka ekranu
    lv_obj_add_flag(label_notification, LV_OBJ_FLAG_HIDDEN);

    // --- IKONY STATUSU ---
    icon_wifi = lv_label_create(ui_main_screen);
    lv_obj_set_style_text_color(icon_wifi, lv_color_make(255, 100, 100), LV_PART_MAIN);
    lv_obj_align(icon_wifi, LV_ALIGN_TOP_RIGHT, -10, 10);
    lv_label_set_text(icon_wifi, LV_SYMBOL_CLOSE);

    icon_phone = lv_label_create(ui_main_screen);
    lv_obj_set_style_text_color(icon_phone, lv_color_make(100, 150, 255), LV_PART_MAIN);
    lv_obj_align(icon_phone, LV_ALIGN_TOP_RIGHT, -50, 10);
    lv_obj_add_flag(icon_phone, LV_OBJ_FLAG_HIDDEN);

    // --- SYMBOL STATUSU POŁĄCZENIA ---
    status_dot = lv_label_create(ui_main_screen);
    lv_label_set_text(status_dot, "X"); // Domyślnie X (brak połączenia)
    lv_obj_set_style_text_color(status_dot, lv_color_make(100, 0, 0), LV_PART_MAIN); // Domyślnie czerwony
    lv_obj_align(status_dot, LV_ALIGN_TOP_MID, 0, 40); // Ta sama pozycja co status - wyśrodkowana
    lv_obj_add_flag(status_dot, LV_OBJ_FLAG_HIDDEN); // Domyślnie ukryta

    // --- ŁUK / ARC ---
    arc_control = lv_arc_create(ui_main_screen);
    lv_obj_set_size(arc_control, 466, 466);
    lv_arc_set_bg_angles(arc_control, 0, 180);
    lv_obj_align(arc_control, LV_ALIGN_CENTER, 0, 0);
    lv_arc_set_range(arc_control, 100, 400);
    lv_arc_set_value(arc_control, (int)(set_temperature * 10));
    lv_obj_add_event_cb(arc_control, arc_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_set_style_arc_width(arc_control, 25, LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc_control, 25, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(arc_control, lv_palette_main(LV_PALETTE_BLUE), LV_PART_MAIN);
    lv_obj_set_style_arc_color(arc_control, lv_color_make(50, 50, 50), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(arc_control, lv_palette_main(LV_PALETTE_BLUE), LV_PART_KNOB);
    lv_obj_set_style_bg_opa(arc_control, LV_OPA_COVER, LV_PART_KNOB);
    lv_obj_set_style_pad_all(arc_control, 10, LV_PART_KNOB);
    lv_obj_clear_flag(arc_control, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(arc_control, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(arc_control, LV_OBJ_FLAG_ADV_HITTEST);

    // --- Label set_temp ---
    label_set_temp = lv_label_create(ui_main_screen);
    lv_obj_set_style_text_color(label_set_temp, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(label_set_temp, &lv_font_montserrat_28_pl, 0); // Większa czcionka
    lv_obj_align(label_set_temp, LV_ALIGN_CENTER, 0, 0);
    update_set_temp_label();
    update_arc_color(arc_control, set_temperature);
    
    // --- Obsługa dotknięć ekranu ---
    lv_obj_clear_flag(ui_main_screen, LV_OBJ_FLAG_SCROLLABLE); // Wyłącz przewijanie
    lv_obj_add_flag(ui_main_screen, LV_OBJ_FLAG_CLICKABLE);    // Włącz klikanie
    lv_obj_add_event_cb(ui_main_screen, screen_event_cb, LV_EVENT_PRESSED, NULL);
    printf("[UI] Screen touch events configured\n");
    
    // --- Ustaw początkowy stan blokady ---
    last_activity_time = lv_tick_get();
    update_screen_locked_state();
}

// ─────────────────────────────
// INTEGRACJA Z BACKENDEM
// ─────────────────────────────
void main_screen_set_target_c(float c) {
    set_temperature = c;
    update_set_temp_label();
    update_arc_color(arc_control, set_temperature);
    if (arc_control) {
        int arc_val = 100 + 400 - (int)(set_temperature * 10); // odwrotna skala
        lv_arc_set_value(arc_control, arc_val);
    }
    printf("[UI] Zmieniono temperaturę zadaną: %.1f°C\n", set_temperature);
}

float main_screen_get_target_c(void) {
    return set_temperature;
}

void main_screen_set_target_c_from_server(float c, const char *source) {
    bool from_app = (source && strcmp(source, "app") == 0);
    
    main_screen_set_target_c(c);
    
    if (from_app) {
        main_screen_show_external_change(true);
        char msg[64];
        snprintf(msg, sizeof(msg), "Nowa temperatura z aplikacji: %.1f°C", c);
        main_screen_show_notification(msg, 3000);
    }
}
