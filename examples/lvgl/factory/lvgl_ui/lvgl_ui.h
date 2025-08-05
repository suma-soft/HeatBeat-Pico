#pragma once

#include "lvgl.h"
#include "screen/main_screen.h"
#include "screen/color_screen.h"
#include "screen/drawing_screen.h"

LV_IMG_DECLARE(lv_logo_wx); // assets/lv_logo_wx.png

void lvgl_ui_init(void);

void _ui_screen_change(lv_obj_t **target, lv_scr_load_anim_t fademode, int spd, int delay, void (*target_init)(void));