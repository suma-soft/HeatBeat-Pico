#ifndef __MAIN_SCREEN_H__
#define __MAIN_SCREEN_H__

#include "../lvgl_ui.h"
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

void update_bme_data(void);

extern lv_obj_t *ui_main_screen;

extern lv_obj_t *label_time;
extern lv_obj_t *label_date;
extern lv_obj_t *label_battery_adc;
extern lv_obj_t *label_battery_voltage;
extern lv_obj_t *label_chip_temp;
extern lv_obj_t *label_chip_freq;
extern lv_obj_t *label_ram_size;
extern lv_obj_t *label_flash_size;
extern lv_obj_t *label_sd_size;

extern lv_obj_t *label_accel_x;
extern lv_obj_t *label_accel_y;
extern lv_obj_t *label_accel_z;

extern lv_obj_t *label_gyro_x;
extern lv_obj_t *label_gyro_y;
extern lv_obj_t *label_gyro_z;

extern lv_obj_t *label_brightness;

extern lv_obj_t *label_temp;
extern lv_obj_t *label_humi;
extern lv_obj_t *label_pres;
extern lv_obj_t *label_set_temp;
extern lv_obj_t *label_target;
extern lv_obj_t *label_status;        // Status komunikacji
extern lv_obj_t *label_notification; // Powiadomienia dla użytkownika
extern lv_obj_t *icon_wifi;          // Ikona WiFi
extern lv_obj_t *icon_phone;         // Ikona telefonu (zmiana z aplikacji)
extern lv_obj_t *btn_up;
extern lv_obj_t *btn_down;

extern float current_temp;
extern int humidity;

void update_labels(void);
void main_screen_init(void);

// Funkcje do wyświetlania statusu i powiadomień
void main_screen_show_status(const char *message, bool is_error);
void main_screen_show_notification(const char *message, int duration_ms);
void main_screen_update_wifi_status(bool connected, int rssi);
void main_screen_show_external_change(bool from_app);
void main_screen_update_timers(void);
void main_screen_update_timers_with_time(uint32_t now);
void main_screen_set_target_c_from_server(float c, const char *source);
void main_screen_set_notification_time(uint32_t time);

// Funkcje blokady ekranu
void check_auto_lock(void);

#ifdef __cplusplus
}
#endif

#endif
