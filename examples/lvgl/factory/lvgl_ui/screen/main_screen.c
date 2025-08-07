#include "main_screen.h"
#include <stdio.h>
#include "lv_font_montserrat_28_pl.h"
#include "../../bme280_port.h"

void update_set_temp_label(void);
static void update_arc_color(lv_obj_t *arc, float temperature);
static lv_color_t interpolate_rgb(int r1, int g1, int b1, int r2, int g2, int b2, float ratio);

// Przechowujemy kolor bazowy dla kropki (ustawiany w update_arc_color)
static lv_color_t knob_base_color;

lv_obj_t *ui_main_screen;
lv_obj_t *label_set_temp;
lv_obj_t *btn_plus;
lv_obj_t *btn_minus;

float set_temperature = 21.0f; // domyślna temperatura
float current_temp = 0;
int humidity = 0;


static lv_obj_t *label_temp;
static lv_obj_t *label_humi;
static lv_obj_t *label_target;
static lv_obj_t *btn_up;
static lv_obj_t *btn_down;

static int target_temp = 22;
static struct bme280_data bme_data;

void update_labels()
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

static lv_color_t knob_base_color;

static void animate_ironman_breath(void *obj, int32_t pad)
{
    lv_obj_t *arc = (lv_obj_t *)obj;

    // Rozmiar kropki (10–16 px)
    lv_obj_set_style_pad_all(arc, pad, LV_PART_KNOB);

   // Normalizacja: pad z zakresu 10–16 → ratio 0.0 – 1.0
    float ratio = (float)(pad - 10) / (16 - 10);

    // Kolory pulsacji:
    // 🔸 Kolor minimalny (ceglasty): RGB(200, 80, 60)
    // 🔴 Kolor maksymalny (pełna czerwień): RGB(255, 50, 50)
    lv_color_t dimmed = interpolate_rgb(200, 80, 60, 255, 50, 50, ratio);

    // Zastosuj jednocześnie na kropce i tle łuku
    lv_obj_set_style_bg_color(arc, dimmed, LV_PART_KNOB);
    lv_obj_set_style_arc_color(arc, dimmed, LV_PART_KNOB);
    lv_obj_set_style_arc_color(arc, dimmed, LV_PART_MAIN);
}

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
        return lv_color_make(100, 200, 255);  // niebieski

    if (temp <= 21.0f)
        return interpolate_rgb(100, 200, 255, 100, 255, 100, (temp - 17.0f) / 4.0f);  // → zielony

    if (temp <= 24.0f)
        return interpolate_rgb(100, 255, 100, 255, 255, 100, (temp - 21.0f) / 3.0f);  // → żółty

    if (temp <= 27.0f)
        return interpolate_rgb(255, 255, 100, 200, 80, 60, (temp - 24.0f) / 3.0f);    // → ceglasta

    if (temp < 30.0f)
        return interpolate_rgb(200, 80, 60, 255, 50, 50, (temp - 27.0f) / 3.0f);      // → jasna czerwień

    return lv_color_make(255, 50, 50);  // pełna czerwień (od 30°C)
}


static void update_arc_color(lv_obj_t *arc, float temperature)
{
    lv_color_t color = interpolate_color(temperature);

    // zapisz kolor bazowy (do animacji)
    knob_base_color = color;

    // Tło łuku
    lv_obj_set_style_arc_color(arc, color, LV_PART_MAIN);

    // Pasek aktywny (stały)
    lv_obj_set_style_arc_color(arc, lv_color_make(40, 40, 40), LV_PART_INDICATOR);

    // Kropka – kolor bazowy
    lv_obj_set_style_arc_color(arc, color, LV_PART_KNOB);
    lv_obj_set_style_bg_color(arc, color, LV_PART_KNOB);

    // Reset stylów (nieprzezroczystość 100%)
    lv_obj_set_style_bg_opa(arc, LV_OPA_COVER, LV_PART_KNOB);
    lv_obj_set_style_arc_opa(arc, LV_OPA_COVER, LV_PART_MAIN);

    if (temperature >= 30.0f)
    {
        // 🌬️ Start efektu Iron Mana
        lv_anim_t a;
        lv_anim_init(&a);
        lv_anim_set_var(&a, arc);
        lv_anim_set_exec_cb(&a, animate_ironman_breath);
        lv_anim_set_values(&a, 10, 16);
        lv_anim_set_time(&a, 1000);            // tempo oddechu
        lv_anim_set_playback_time(&a, 1000);
        lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
        lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
        lv_anim_start(&a);
    }
    else
    {
        // ❌ Zatrzymaj efekt
        lv_anim_del(arc, animate_ironman_breath);
        lv_obj_set_style_pad_all(arc, 10, LV_PART_KNOB);
        lv_obj_set_style_arc_color(arc, color, LV_PART_MAIN);
        lv_obj_set_style_arc_color(arc, color, LV_PART_KNOB);
        lv_obj_set_style_bg_color(arc, color, LV_PART_KNOB);
    }
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

//void update_bme_data() {
//    int8_t result = bme280_read_data(&bme_data);
//    if (result == BME280_OK) {
//        current_temp = bme_data.temperature;
//        humidity = (int)(bme_data.humidity + 0.5f);  // zaokrąglamy
//        update_labels();
//    } else {
//        printf("❌ Błąd odczytu danych z BME280: %d\n", result);
//    }
//}

void main_screen_init(void)
{
    //int8_t status = bme280_init_default();
    //if (status == BME280_OK) {
    //    printf("✅ BME280 zainicjalizowany poprawnie\n");
    //    update_bme_data();
    //} else {
    //    printf(" Błąd inicjalizacji BME280: %d\n", status);
    // }

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
    
    //lv_timer_create((lv_timer_cb_t)update_bme_data, 5000, NULL);

}