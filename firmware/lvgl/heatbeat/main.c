#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/stdio_usb.h"
#include "lvgl.h"
#include "bme280_port.h"
#include "../lv_port/lv_port_disp.h"
#include "../lv_port/lv_port_indev.h"
#include "bsp_i2c.h"
#include "bsp_pcf85063.h"
#include "lvgl_ui/screen/main_screen.h"

#define LVGL_TICK_MS 5
#define DISP_HOR_RES 466
#define DISP_VER_RES 466

// LVGL tick timer
static bool tick_cb(struct repeating_timer *t) { lv_tick_inc(LVGL_TICK_MS); return true; }

// Print free RAM (RP2040-specific)
extern char __StackLimit, __bss_end__;
static void print_free_ram(const char* msg) {
    uint32_t free_ram = (uint32_t)&__StackLimit - (uint32_t)&__bss_end__;
    printf("[RAM] %s: Wolna RAM: %lu bajtów\n", msg, free_ram);
}

int main(void) {
    stdio_usb_init();
    printf("\r\n--- HeatBeat-Pico start! ---\r\n");

    // Oczekiwanie na połączenie z Putty
    //printf("Czekam 5 sekund na połączenie przez Putty...\r\n");
    //sleep_ms(20000);

    // Inicjalizacja hardware
    bsp_i2c_init();
    bsp_pcf85063_init();

    lv_init();
    lv_port_disp_init(DISP_HOR_RES, DISP_VER_RES, 0, false);
    lv_port_indev_init(DISP_HOR_RES, DISP_VER_RES, 0);

    // Inicjalizacja czujnika BME280
    bme280_init_default();

    // Inicjalizacja głównego ekranu UI
    main_screen_init();
    lv_scr_load(ui_main_screen);

    // Timer LVGL
    static struct repeating_timer t;
    add_repeating_timer_ms(LVGL_TICK_MS, tick_cb, NULL, &t);

    // Timery do update'ów
    uint32_t last_read = to_ms_since_boot(get_absolute_time());
    uint32_t last_time = last_read;
    struct tm now_tm;
    struct bme280_data bme_data;

    while (true) {
        lv_timer_handler();
        sleep_ms(LVGL_TICK_MS);

        uint32_t now = to_ms_since_boot(get_absolute_time());

        // Aktualizacja czasu na ekranie
        if (now - last_time > 1000) {
            bsp_pcf85063_get_time(&now_tm);
            char buf[32];
            snprintf(buf, sizeof(buf), "%02d:%02d:%02d", now_tm.tm_hour, now_tm.tm_min, now_tm.tm_sec);
            if (label_time) lv_label_set_text(label_time, buf);
            last_time = now;
        }

        // Aktualizacja wartości z BME280 co 2 sekundy
        if (now - last_read > 2000) {
            if (bme280_read_data(&bme_data) == 0) {
                extern float current_temp;
                extern int humidity;
                extern float pressure;
                current_temp = bme_data.temperature;
                humidity = (int)(bme_data.humidity + 0.5f);
                pressure = bme_data.pressure;
                extern void update_labels(void);
                update_labels();
            }
            last_read = now;
        }
    }
}