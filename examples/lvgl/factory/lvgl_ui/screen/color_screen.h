#ifndef __COLOR_SCREEN_H__
#define __COLOR_SCREEN_H__
#include "../lvgl_ui.h"


extern lv_obj_t *ui_color_screen;
extern lv_timer_t *color_screen_change_timer;

void color_screen_init(void);

#endif