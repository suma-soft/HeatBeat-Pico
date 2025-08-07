#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/stdio_usb.h"
#include "lvgl.h"
#include "bme280_port.h"

#include "../lv_port/lv_port_disp.h"
#include "../lv_port/lv_port_indev.h"

#include "bsp_i2c.h"
#include "bsp_pcf85063.h"
#include "bsp_battery.h"

#include "hardware/adc.h"
#include "hardware/clocks.h"
#include "hardware/pll.h"
#include "hardware/structs/pll.h"
#include "hardware/structs/clocks.h"

#define LVGL_TICK_MS 5

// Funkcja ustawiająca zegar CPU – przeniesiona z poprzedniego UI
void set_cpu_clock(uint32_t freq_Mhz)
{
    set_sys_clock_khz(freq_Mhz * 1000, true);
    clock_configure(
        clk_peri,
        0,
        CLOCKS_CLK_PERI_CTRL_AUXSRC_VALUE_CLKSRC_PLL_SYS,
        freq_Mhz * 1000 * 1000,
        freq_Mhz * 1000 * 1000
    );
}

// Callback timera LVGL
static bool tick_cb(struct repeating_timer *t) {
    lv_tick_inc(LVGL_TICK_MS);
    return true;
}

int main(void) {
    stdio_usb_init();
    printf("=== Program START (USB active) ===\n");
    printf("Startuję inicjalizację...\n");

    // --- Ustawienie zegara CPU na 250 MHz
    set_cpu_clock(250);

    // --- Inicjalizacja ADC i modułu baterii (z poprzedniego UI)
    adc_init();
    bsp_battery_init();
    adc_set_temp_sensor_enabled(true);

    // --- Inicjalizacja I2C1 (wspólne dla BME i RTC)
    bsp_i2c_init();

    // --- Inicjalizacja RTC
    bsp_pcf85063_init();

    // --- Inicjalizacja LVGL i wyświetlacza
    lv_init();
    lv_port_disp_init(466, 466, 0, false);
    lv_port_indev_init(466, 466, 0);

    // --- Inicjalizacja BME280
    int8_t bme_status = bme280_init_default();
    printf("Po bme280_init_default, status: %d\n", bme_status);

    // --- Ekran testowy LVGL
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_t *lbl = lv_label_create(scr);
    lv_label_set_text(lbl, "LVGL+BME+RTC test");
    lv_obj_center(lbl);

    // Dodaj label na czas
    lv_obj_t *lbl_time = lv_label_create(scr);
    lv_obj_align(lbl_time, LV_ALIGN_TOP_MID, 0, 20);

    lv_scr_load(scr);

    // --- Timer LVGL
    static struct repeating_timer t;
    add_repeating_timer_ms(LVGL_TICK_MS, tick_cb, NULL, &t);

    // --- Pętla główna + co 2s odczyt BME280, co sekundę update czasu ---
    struct bme280_data data;
    uint32_t last_read = to_ms_since_boot(get_absolute_time());
    uint32_t last_time = last_read;

    struct tm now_tm;

    while (true) {
        lv_timer_handler();
        sleep_ms(LVGL_TICK_MS);

        uint32_t now = to_ms_since_boot(get_absolute_time());

        if (now - last_time > 1000) {
            // Odczytaj czas z RTC i pokaż na labelu
            bsp_pcf85063_get_time(&now_tm);
            char buf[32];
            snprintf(buf, sizeof(buf), "%02d:%02d:%02d",
                     now_tm.tm_hour, now_tm.tm_min, now_tm.tm_sec);
            lv_label_set_text(lbl_time, buf);
            last_time = now;
        }

        if (now - last_read > 2000) {
            int8_t res = bme280_read_data(&data);
            printf("BME280 odczyt: %d\n", res);
            if (res == 0) {
                char buf[64];
                snprintf(buf, sizeof(buf), "T:%.2fC H:%.1f%%",
                         data.temperature, data.humidity);
                lv_label_set_text(lbl, buf);
            } else {
                lv_label_set_text(lbl, "BME error!");
            }
            last_read = now;
        }
    }
}
