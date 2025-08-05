/**
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdio.h>
#include "pico/stdlib.h"
#include "../lv_port/lv_port_disp.h"
#include "../lv_port/lv_port_indev.h"
#include "../lv_port/lv_port_fs.h"
#include "demos/lv_demos.h"
#include "bsp_i2c.h"
#include "bsp_qmi8658.h"
#include "bsp_pcf85063.h"
#include "bsp_battery.h"

#include "hardware/adc.h"
#include "hardware/pll.h"
#include "hardware/clocks.h"
#include "hardware/structs/pll.h"
#include "hardware/structs/clocks.h"

#include "lvgl_ui/lvgl_ui.h"

#define LVGL_TICK_PERIOD_MS 5

#define DISP_HOR_RES 466
#define DISP_VER_RES 466


void set_cpu_clock(uint32_t freq_Mhz)
{
    set_sys_clock_khz(freq_Mhz * 1000, true);
    clock_configure(
        clk_peri,
        0,
        CLOCKS_CLK_PERI_CTRL_AUXSRC_VALUE_CLKSRC_PLL_SYS,
        freq_Mhz * 1000 * 1000,
        freq_Mhz * 1000 * 1000);
}


static bool repeating_lvgl_timer_cb(struct repeating_timer *t)
{
    lv_tick_inc(LVGL_TICK_PERIOD_MS);
    return true;
}

int main()
{
    struct tm now_tm;
    stdio_init_all();
    set_cpu_clock(250);
    adc_init();
    bsp_battery_init();
    adc_set_temp_sensor_enabled(true);
    bsp_i2c_init();
    bsp_qmi8658_init();
    bsp_pcf85063_init();
    bsp_pcf85063_get_time(&now_tm);
    if (now_tm.tm_year < 125 || now_tm.tm_year > 130)
    {
        now_tm.tm_year = 2025 - 1900; // The year starts from 1900
        now_tm.tm_mon = 1 - 1;        // Months start from 0 (November = 10)
        now_tm.tm_mday = 1;           // Day of the month
        now_tm.tm_hour = 12;          // Hour
        now_tm.tm_min = 0;            // Minute
        now_tm.tm_sec = 0;            // Second
        now_tm.tm_isdst = -1;         // Automatically detect daylight saving time
        bsp_pcf85063_set_time(&now_tm);
    }
    lv_init();
    lv_port_fs_init();
    lv_port_disp_init(DISP_HOR_RES, DISP_VER_RES, 0, false);
    lv_port_indev_init(DISP_HOR_RES, DISP_VER_RES, 0);
    static struct repeating_timer lvgl_timer;
    add_repeating_timer_ms(LVGL_TICK_PERIOD_MS, repeating_lvgl_timer_cb, NULL, &lvgl_timer);
    // lv_demo_widgets();
    //lvgl_ui_init();
    main_screen_init();           // tworzy ekran
    lv_scr_load(ui_main_screen); // pokazuje go
    
    while (true)
    {
        lv_timer_handler();
        sleep_ms(LVGL_TICK_PERIOD_MS);
    }
}
