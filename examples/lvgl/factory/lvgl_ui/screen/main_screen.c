#include "main_screen.h"
#include <stdio.h>
#include "lv_font_montserrat_28_pl.h"

void update_set_temp_label(void);
static void update_arc_color(lv_obj_t *arc, float temperature);

lv_obj_t *ui_main_screen;
lv_obj_t *label_set_temp;
lv_obj_t *btn_plus;
lv_obj_t *btn_minus;

float set_temperature = 21.0f; // domyślna temperatura

static lv_obj_t *label_temp;
static lv_obj_t *label_humi;
static lv_obj_t *label_target;
static lv_obj_t *btn_up;
static lv_obj_t *btn_down;

static int target_temp = 22;
static float current_temp = 21.4;
static int humidity = 44;

static void update_labels()
{
    char buf[32];

    snprintf(buf, sizeof(buf), "Temperatura: %.1f°C", current_temp);
    lv_label_set_text(label_temp, buf);

    snprintf(buf, sizeof(buf), "Wilgotność: %d%%", humidity);
    lv_label_set_text(label_humi, buf);

    snprintf(buf, sizeof(buf), "Zadana: %.1f°C", target_temp);
    lv_label_set_text(label_target, buf);
}

static void btn_event_cb(lv_event_t *e) {
    lv_obj_t *btn = lv_event_get_target(e);
    if (btn == btn_up) {
        target_temp++;
    } else if (btn == btn_down) {
        target_temp--;
    }
    update_labels();
}

void slider_event_cb(lv_event_t *e) {
    lv_obj_t *arc = lv_event_get_target(e);
    int val = lv_arc_get_value(arc);
    set_temperature = val / 10.0f;
    update_set_temp_label();
}

void update_set_temp_label(void) {
    static char buf[32];
    snprintf(buf, sizeof(buf), "Zadana: %.1f°C", set_temperature);
    lv_label_set_text(label_set_temp, buf);
}

static void update_arc_color(lv_obj_t *arc, float temperature)
{
    lv_color_t color;

    if (temperature <= 15.0f) {
        color = lv_color_make(100, 200, 255); // ❄️ niebieski
    } else if (temperature <= 25.0f) {
        color = lv_color_make(100, 255, 100); // 🌿 zielony
    } else if (temperature <= 30.0f) {
        color = lv_color_make(255, 180, 50);  // ☀️ pomarańczowy
    } else {
        color = lv_color_make(255, 50, 50);   // 🔥 czerwony
    }

    // 🔄 Tylko tło zmienne
    lv_obj_set_style_arc_color(arc, color, LV_PART_MAIN);              // zmieniamy tło
    lv_obj_set_style_arc_color(arc, lv_color_make(80, 80, 80), LV_PART_INDICATOR); // wskaźnik stały
}

void arc_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *arc = lv_event_get_target(e);
    if (code == LV_EVENT_VALUE_CHANGED)
    {
        int val = lv_arc_get_value(arc);

        // 🔄 Odwrócenie – min + max - val
        int reversed_val = 100 + 400 - val;

        set_temperature = reversed_val / 10.0f;
        update_set_temp_label();
        update_arc_color(arc, set_temperature);
    }
}



void main_screen_init(void)
{
    ui_main_screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(ui_main_screen, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_text_font(ui_main_screen, &lv_font_montserrat_28_pl, 0);


    lv_scr_load(ui_main_screen); // <- KLUCZOWE

    // Etykieta temperatury
    label_temp = lv_label_create(ui_main_screen);
    lv_label_set_text(label_temp, "Temperatura: 21.4°C");
    lv_obj_set_style_text_color(label_temp, lv_color_white(), LV_PART_MAIN);
    lv_obj_align(label_temp, LV_ALIGN_TOP_MID, 0, 40);

    // Etykieta wilgotności
    label_humi = lv_label_create(ui_main_screen);
    lv_label_set_text(label_humi, "Wilgotność: 44%");
    lv_obj_set_style_text_color(label_humi, lv_color_white(), LV_PART_MAIN);
    lv_obj_align(label_humi, LV_ALIGN_TOP_MID, 0, 90);

    lv_obj_t *arc = lv_arc_create(ui_main_screen);

    // Ustaw rozmiar
    lv_obj_set_size(arc, 466, 466);

    // Zamiast dolnej ćwiartki, użyj górnej i obróć
    lv_arc_set_bg_angles(arc, 0, 180); // 0° do 180° (górna część)

    // Pozycjonowanie
    lv_obj_align(arc, LV_ALIGN_CENTER, 0, 0);  // niżej na ekranie

    // Zakres i wartość
    lv_arc_set_range(arc, 100, 400);  // 10.0°C – 40.0°C (x10)
    lv_arc_set_value(arc, (int)(set_temperature * 10));

    // Callback
    lv_obj_add_event_cb(arc, arc_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    // Styl
    lv_obj_set_style_arc_width(arc, 25, LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc, 25, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(arc, lv_palette_main(LV_PALETTE_BLUE), LV_PART_MAIN);
    lv_obj_set_style_arc_color(arc, lv_color_make(50, 50, 50), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(arc, lv_palette_main(LV_PALETTE_BLUE), LV_PART_KNOB);
    lv_obj_set_style_bg_opa(arc, LV_OPA_COVER, LV_PART_KNOB);
    lv_obj_set_style_pad_all(arc, 10, LV_PART_KNOB);

    // Kropka (knob)
    lv_obj_set_style_bg_color(arc, lv_palette_main(LV_PALETTE_BLUE), LV_PART_KNOB);
    lv_obj_set_style_bg_opa(arc, LV_OPA_COVER, LV_PART_KNOB);

    //interaktywny łuk
    lv_obj_clear_flag(arc, LV_OBJ_FLAG_CLICKABLE); // just in case
    lv_obj_add_flag(arc, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(arc, LV_OBJ_FLAG_ADV_HITTEST); // lepsze dopasowanie dotyku

    // Etykieta zadanej temperatury do regulacji
    label_set_temp = lv_label_create(ui_main_screen);
    lv_obj_set_style_text_color(label_set_temp, lv_color_white(), LV_PART_MAIN);
    lv_obj_align(label_set_temp, LV_ALIGN_CENTER, 0, 0); // środek okręgu
    update_set_temp_label();
    update_arc_color(arc, set_temperature);
    
}