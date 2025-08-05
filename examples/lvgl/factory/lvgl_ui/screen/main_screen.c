#include "main_screen.h"
#include <stdio.h>
#include "lv_font_montserrat_28_pl.h"

lv_obj_t *ui_main_screen;



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

    snprintf(buf, sizeof(buf), "Zadana: %d°C", target_temp);
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

void main_screen_init(void)
{
    ui_main_screen = lv_obj_create(NULL);
    lv_obj_set_style_text_font(ui_main_screen, &lv_font_montserrat_28_pl, 0);  // ← to jest dobre miejsce
    lv_obj_t *scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, lv_color_black(), LV_PART_MAIN);

    label_temp = lv_label_create(scr);
    lv_obj_set_style_text_color(label_temp, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(label_temp, &lv_font_montserrat_28_pl, LV_PART_MAIN);
    lv_obj_align(label_temp, LV_ALIGN_TOP_MID, 0, 40);

    label_humi = lv_label_create(scr);
    lv_obj_set_style_text_color(label_humi, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(label_humi, &lv_font_montserrat_28_pl, LV_PART_MAIN);
    lv_obj_align(label_humi, LV_ALIGN_TOP_MID, 0, 90);

    label_target = lv_label_create(scr);
    lv_obj_set_style_text_color(label_target, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_font(label_target, &lv_font_montserrat_28_pl, LV_PART_MAIN);
    lv_obj_align(label_target, LV_ALIGN_TOP_MID, 0, 140);

    btn_up = lv_btn_create(scr);
    lv_obj_align(btn_up, LV_ALIGN_BOTTOM_LEFT, 40, -20);
    lv_obj_add_event_cb(btn_up, btn_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *label_plus = lv_label_create(btn_up);
    lv_label_set_text(label_plus, "+");

    btn_down = lv_btn_create(scr);
    lv_obj_align(btn_down, LV_ALIGN_BOTTOM_RIGHT, -40, -20);
    lv_obj_add_event_cb(btn_down, btn_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *label_minus = lv_label_create(btn_down);
    lv_label_set_text(label_minus, "-");

    update_labels();
}

