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
#include "bme280_port.h"
#include "lvgl_ui/lvgl_ui.h"
#include "lvgl_ui/screen/main_screen.h"
#include "bme280/bme280.h"

// ⬇️ Dodane — używamy zmiennych z main_screen.c
extern float current_temp;
extern int humidity;
extern struct bme280_data bme_data;

#define LVGL_TICK_PERIOD_MS 5
#define DISP_HOR_RES 466
#define DISP_VER_RES 466

void update_bme_data(void);

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
    stdio_usb_init();
    sleep_ms(3000);

    int8_t status = bme280_init_default();
    if (status == BME280_OK) {
        printf("✅ BME280 zainicjalizowany poprawnie\n");
        update_bme_data();  // od razu pierwsze dane
    } else {
        printf("❌ Błąd inicjalizacji BME280: %d\n", status);
    }

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
        now_tm.tm_year = 2025 - 1900;
        now_tm.tm_mon = 1 - 1;
        now_tm.tm_mday = 1;
        now_tm.tm_hour = 12;
        now_tm.tm_min = 0;
        now_tm.tm_sec = 0;
        now_tm.tm_isdst = -1;
        bsp_pcf85063_set_time(&now_tm);
    }

    lv_init();
    lv_port_fs_init();
    lv_port_disp_init(DISP_HOR_RES, DISP_VER_RES, 0, false);
    lv_port_indev_init(DISP_HOR_RES, DISP_VER_RES, 0);

    static struct repeating_timer lvgl_timer;
    add_repeating_timer_ms(LVGL_TICK_PERIOD_MS, repeating_lvgl_timer_cb, NULL, &lvgl_timer);

    main_screen_init();
    lv_scr_load(ui_main_screen);

    uint32_t last_bme_read = to_ms_since_boot(get_absolute_time());

    while (true)
    {
        lv_timer_handler();
        sleep_ms(LVGL_TICK_PERIOD_MS);

        uint32_t now = to_ms_since_boot(get_absolute_time());
        if (now - last_bme_read > 1000)
        {
            update_bme_data();
            last_bme_read = now;
        }
    }
}

void update_bme_data(void)
{
    struct bme280_data comp_data;
    int8_t res = bme280_read_data(&comp_data);

    if (res == BME280_OK) {
        current_temp = comp_data.temperature;
        humidity = (int)(comp_data.humidity + 0.5f);
        bme_data = comp_data; // <- zapisujemy dane także do bme_data (np. ciśnienie)
        update_labels();
        printf("🌡️ %.2f°C  💧 %d%%  ⬇️ %.2f hPa\n", current_temp, humidity, bme_data.pressure / 100.0f);
    } else {
        printf("❌ Błąd odczytu danych z BME280: %d\n", res);
    }
}
