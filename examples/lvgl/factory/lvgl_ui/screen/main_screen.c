#include "main_screen.h"
#include <stdio.h>
#include "lv_font_montserrat_28_pl.h"

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

void update_set_temp_label() {
    static char buf[16];
    snprintf(buf, sizeof(buf), "Zadana: %.1f°C", set_temperature);
    lv_label_set_text(label_set_temp, buf);
}

void btn_plus_event_cb(lv_event_t *e) {
    if (set_temperature < 40.0f) {
        set_temperature += 0.5f;
        update_set_temp_label();
    }
}

void btn_minus_event_cb(lv_event_t *e) {
    if (set_temperature > 10.0f) {
        set_temperature -= 0.5f;
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

    // Etykieta zadanej temperatury
    label_target = lv_label_create(ui_main_screen);
    lv_label_set_text(label_target, "Zadana: 22°C");
    lv_obj_set_style_text_color(label_target, lv_color_white(), LV_PART_MAIN);
    lv_obj_align(label_target, LV_ALIGN_TOP_MID, 0, 140);

    // Przycisk minus
    btn_minus = lv_btn_create(ui_main_screen);
    lv_obj_set_size(btn_minus, 60, 60);
    lv_obj_align(btn_minus, LV_ALIGN_CENTER, -80, 100);  // ← bezpiecznie nad dolną krawędzią
    lv_obj_add_event_cb(btn_minus, btn_minus_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *label_minus = lv_label_create(btn_minus);
    lv_label_set_text(label_minus, "-");
    lv_obj_center(label_minus);
    lv_obj_set_style_text_font(label_minus, &lv_font_montserrat_28_pl, LV_PART_MAIN);

    // Przycisk plus
    btn_plus = lv_btn_create(ui_main_screen);
    lv_obj_set_size(btn_plus, 60, 60);
    lv_obj_align(btn_plus, LV_ALIGN_CENTER, 80, 100);   // ← symetrycznie
    lv_obj_add_event_cb(btn_plus, btn_plus_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *label_plus = lv_label_create(btn_plus);
    lv_label_set_text(label_plus, "+");
    lv_obj_center(label_plus);
    lv_obj_set_style_text_font(label_plus, &lv_font_montserrat_28_pl, LV_PART_MAIN);

    // Etykieta zadanej temperatury do regulacji
    label_set_temp = lv_label_create(ui_main_screen);
    lv_obj_set_style_text_color(label_set_temp, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(label_set_temp, &lv_font_montserrat_28_pl, LV_PART_MAIN);
    lv_obj_align(label_set_temp, LV_ALIGN_CENTER, 0, 40);
    update_set_temp_label();
}

