#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/stdio_usb.h"
#include "lvgl.h"
#include "bme280_port.h"
#include "../lv_port/lv_port_disp.h"
#include "../lv_port/lv_port_indev.h"
#include "bsp_i2c.h"
#include "bsp_pcf85063.h"
#include "lvgl_ui/lvgl_ui.h"
#include "lvgl_ui/screen/main_screen.h"

#define LVGL_TICK_MS 5
#define DISP_HOR_RES 466
#define DISP_VER_RES 466

// Timer do obsługi LVGL ticka
static bool tick_cb(struct repeating_timer *t) { lv_tick_inc(LVGL_TICK_MS); return true; }

// --- Do szacowania wolnej RAM na RP2040 ---
extern char __StackLimit, __bss_end__;

// --- Funkcja drukująca RAM ---
static void print_free_ram(const char* msg) {
    uint32_t free_ram = (uint32_t)&__StackLimit - (uint32_t)&__bss_end__;
    printf("[RAM] %s: Wolna RAM: %lu bajtów\n", msg, free_ram);
}

int main(void) {
    // --- Inicjalizacja USB do komunikacji printf ---
    stdio_usb_init();
    printf("\r\n--- HeatBeat-Pico start! ---\r\n");

    // --- Odliczanie 20 sekund na podłączenie PuTTY ---
    printf("Czekam 20 sekund na połączenie przez Putty...\r\n");
    for (int i = 20; i > 0; i--) {
        printf("Start za %2d s\r\n", i);
        sleep_ms(1000);
    }
    printf("Zaczynam inicjalizację!\r\n");
    print_free_ram("Po oczekiwaniu startowym");

    // --- Inicjalizacja magistrali I2C (musi być przed urządzeniami na tej szynie) ---
    bsp_i2c_init();
    printf("bsp_i2c_init OK\r\n");
    print_free_ram("Po bsp_i2c_init");

    // --- Odczekaj na rozruch USB oraz I2C (stabilność magistrali, typowe dla Pico) ---
    sleep_ms(300);
    printf("sleep po I2C/USB OK\r\n");
    print_free_ram("Po sleep 300ms");

    // --- Inicjalizacja zegara czasu rzeczywistego RTC ---
    bsp_pcf85063_init();
    printf("bsp_pcf85063_init (RTC) OK\r\n");
    print_free_ram("Po bsp_pcf85063_init");

    // --- Inicjalizacja biblioteki LVGL ---
    lv_init();
    printf("lv_init OK\r\n");
    print_free_ram("Po lv_init");

    // --- Inicjalizacja wyświetlacza pod LVGL (rozdzielczość i parametry wg używanego panelu) ---
    lv_port_disp_init(DISP_HOR_RES, DISP_VER_RES, 0, false);
    printf("lv_port_disp_init OK\r\n");
    print_free_ram("Po lv_port_disp_init");

    // --- Inicjalizacja wejść (np. dotyk, enkoder) do obsługi przez LVGL ---
    lv_port_indev_init(DISP_HOR_RES, DISP_VER_RES, 0);
    printf("lv_port_indev_init OK\r\n");
    print_free_ram("Po lv_port_indev_init");

    // --- Odczekaj na pełną inicjalizację wyświetlacza (niektóre panele potrzebują czasu po LVGL init) ---
    sleep_ms(200);
    printf("sleep po init wyświetlacza OK\r\n");
    print_free_ram("Po sleep 200ms");

    // --- Inicjalizacja czujnika BME280 (temperatura, wilgotność, ciśnienie) ---
    printf("=== INICJALIZACJA BME280 ===\n");
    printf("Przed bme280_init_default()\n");
    int8_t status = bme280_init_default();
    printf("Po bme280_init_default(), status = %d\n", status);

    if (status == 0) {
        printf("✅ BME280 zainicjalizowany poprawnie!\n");
    } else {
        printf("❌ Błąd inicjalizacji BME280: %d\n", status);
    }
    printf("============================\n");
    print_free_ram("Po bme280_init_default");

    // --- Inicjalizacja głównego ekranu UI (np. wygenerowanego przez SquareLine Studio) ---
    main_screen_init();
    printf("main_screen_init OK\r\n");
    print_free_ram("Po main_screen_init");

    // --- Załaduj główny ekran jako aktywny (wskaźnik ui_main_screen generowany przez generator lub twój plik) ---
    lv_scr_load(ui_main_screen);
    printf("lv_scr_load OK\r\n");
    print_free_ram("Po lv_scr_load");

    // --- Odpoczynek po inicjalizacji etykiety czasu na głównym ekranie ---
    sleep_ms(500);
    printf("sleep po lv_scr_load OK\r\n");
    print_free_ram("Po sleep 500ms");

    // --- Inicjalizacja timer'a do obsługi ticków LVGL ---
    printf("Przed add_repeating_timer_ms\r\n");
    static struct repeating_timer t;
    add_repeating_timer_ms(LVGL_TICK_MS, tick_cb, NULL, &t);
    printf("add_repeating_timer_ms OK\r\n");
    print_free_ram("Po add_repeating_timer_ms");

    // --- Przed wejściem do pętli głównej ---
    printf("Przed wejściem do pętli głównej\r\n");

    // --- Zmienne pomocnicze do pętli głównej ---
    uint32_t last_read = to_ms_since_boot(get_absolute_time());
    uint32_t last_time = last_read;
    uint32_t mem_print_timer = last_read;
    struct tm now_tm;
    struct bme280_data bme_data;

    // --- Pętla główna: odświeżanie LVGL i cykliczne odczyty czujników ---
    while (true) {
        // --- Obsługa LVGL ---
        lv_timer_handler();
        sleep_ms(LVGL_TICK_MS);

        uint32_t now = to_ms_since_boot(get_absolute_time());

        // --- Drukowanie RAM co 5 sekund ---
        if (now - mem_print_timer > 5000) {
            print_free_ram("W pętli głównej");
            mem_print_timer = now;
        }

        // --- Aktualizacja czasu co sekundę ---
        if (now - last_time > 1000) {
            bsp_pcf85063_get_time(&now_tm);
            char buf[32];
            snprintf(buf, sizeof(buf), "%02d:%02d:%02d", now_tm.tm_hour, now_tm.tm_min, now_tm.tm_sec);
            // Przypisz do labela na ekranie głównym
            if (label_time) {
                lv_label_set_text(label_time, buf);
            }
            last_time = now;
        }

        // --- Aktualizacja wartości z BME280 co 2 sekundy ---
        if (now - last_read > 2000) {
            int8_t res = bme280_read_data(&bme_data);
            if (res == 0) {
                // Przypisz do globalnych zmiennych main_screen, by update_labels() mogło je pokazać
                extern float current_temp;
                extern int humidity;
                extern float pressure; // <-- DODAJ TO!
                current_temp = bme_data.temperature;
                humidity = (int)(bme_data.humidity + 0.5f);
                pressure = bme_data.pressure; // <-- DODAJ TO!
                // wywołaj aktualizację etykiet na ekranie
                extern void update_labels(void);
                update_labels();
            }
            last_read = now;
        }
    }
}