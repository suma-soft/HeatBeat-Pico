#include "color_screen.h"

#define BG_COLOR_MAX 3

lv_obj_t *ui_color_screen = NULL;
lv_timer_t *color_screen_change_timer = NULL;

uint32_t bg_color_arr[BG_COLOR_MAX] = {0xff0000, 0x00ff00, 0x0000ff};
uint16_t bg_color_index = 0;

// 定时器回调函数
static void timer_callback(lv_timer_t *timer)
{
    // 获取当前屏幕
    lv_obj_t *scr = lv_scr_act();

    lv_obj_set_style_bg_color(scr, lv_color_hex(bg_color_arr[bg_color_index]), LV_PART_MAIN);
    if (bg_color_index++ >= BG_COLOR_MAX - 1)
    {
        bg_color_index = 0;
    }
}


static void swipe_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_GESTURE)
    {
        lv_dir_t dir = lv_indev_get_gesture_dir(lv_indev_get_act());

        if (dir == LV_DIR_LEFT)
        {
            lv_timer_pause(color_screen_change_timer);
            // 等待触摸屏释放
            lv_indev_wait_release(lv_indev_get_act());
            // 跳转到颜色测试界面
            _ui_screen_change(&ui_main_screen, LV_SCR_LOAD_ANIM_FADE_ON, 500, 0, &main_screen_init);
        }
    }
}

void color_screen_init(void)
{
    ui_color_screen = lv_obj_create(NULL);
    lv_obj_clear_flag(ui_color_screen, LV_OBJ_FLAG_SCROLLABLE); /// Flags
    // lv_obj_set_style_bg_color(ui_color_screen, lv_color_hex(0x000000),LV_PART_MAIN | LV_STATE_DEFAULT);
    color_screen_change_timer = lv_timer_create(timer_callback, 1000, NULL);
    lv_timer_pause(color_screen_change_timer);
    lv_obj_add_event_cb(ui_color_screen, swipe_event_cb, LV_EVENT_GESTURE, NULL);
}