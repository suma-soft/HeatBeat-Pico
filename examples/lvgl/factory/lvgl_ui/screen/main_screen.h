#ifndef __MAIN_SCREEN_H__
#define __MAIN_SCREEN_H__

#include "../lvgl_ui.h"
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

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

extern float current_temp;
extern int humidity;

void update_labels(void);
void main_screen_init(void);

#ifdef __cplusplus
}
#endif

#endif
