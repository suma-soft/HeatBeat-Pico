#include "main_screen.h"
#include <stdio.h>
#include "lv_font_montserrat_28_pl.h"

void update_set_temp_label(void);

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

static void slider_event_cb(lv_event_t *e) {
    lv_obj_t *arc = lv_event_get_target(e);
    int value = lv_arc_get_value(arc);
    set_temperature = value / 10.0f;
}

void update_set_temp_label() {
    static char buf[32];
    snprintf(buf, sizeof(buf), "Zadana: %.1f°C", set_temperature);
    lv_label_set_text(label_set_temp, buf);
}


void arc_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);  // 🟢 pobieramy typ zdarzenia
    lv_obj_t *arc = lv_event_get_target(e);       // 🟢 obiekt, który wywołał event

    if (code == LV_EVENT_VALUE_CHANGED)
    {
        int val = lv_arc_get_value(arc);          // 🟢 pobieramy wartość z łuku
        set_temperature = val / 10.0f;
        update_set_temp_label();
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

    // Tworzenie łuku
    lv_obj_t *arc = lv_arc_create(ui_main_screen); 
    lv_obj_set_size(arc, 320, 320);
    lv_obj_align(arc, LV_ALIGN_CENTER, 0, 50); // wyśrodkowany poziomo, przesunięty w dół

    // Zakres i wartość (x10 dla wartości połówkowych)
    lv_arc_set_range(arc, 100, 400);
    lv_arc_set_value(arc, (int)(set_temperature * 10));

    // Zakres widocznego łuku (90° – ćwiartka): od 135° do 225°
    lv_arc_set_angles(arc, 135, 225);

    // tło łuku
    lv_obj_set_style_arc_color(arc, lv_color_make(80, 80, 80), LV_PART_MAIN | LV_STATE_DEFAULT);

    // gałka łuku
    lv_obj_set_style_bg_color(arc, lv_color_white(), LV_PART_KNOB | LV_STATE_DEFAULT);

    // Callback
    lv_obj_add_event_cb(arc, arc_event_cb, LV_EVENT_VALUE_CHANGED, NULL);  // ✅ DZIAŁA w LVGL v8

    // Etykieta zadanej temperatury do regulacji
    label_set_temp = lv_label_create(ui_main_screen);
    lv_obj_set_style_text_color(label_set_temp, lv_color_white(), LV_PART_MAIN);
    lv_obj_align(label_set_temp, LV_ALIGN_CENTER, 0, 0); // środek okręgu
    update_set_temp_label();
}

