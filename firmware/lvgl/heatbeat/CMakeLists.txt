# firmware/lvgl/heatbeat/CMakeLists.txt

add_executable(heatbeat
    main.c
    bme280_port.c
    ../lv_port/lv_port_disp.c
    ../lv_port/lv_port_indev.c
    ../lv_port/lv_port_fs.c
    font/lv_font_montserrat_28_pl.c
    net/hb_http.c

)

# Nagłówki (tu m.in. lwipopts.h)
target_include_directories(heatbeat PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/bme280
    ${CMAKE_CURRENT_SOURCE_DIR}/font
    ${CMAKE_CURRENT_SOURCE_DIR}/lvgl_ui
    ${CMAKE_CURRENT_SOURCE_DIR}/lvgl_ui/screen
    ${CMAKE_CURRENT_SOURCE_DIR}/net
    ${CMAKE_CURRENT_SOURCE_DIR}/../lv_port
)

# ========= PRZYWRACANIE DZIAŁAJĄCEGO WIFI =========
option(TEMP_DISABLE_WIFI "Temporarily disable WiFi for UI testing" OFF)

# ========= Wi-Fi (CYW43 / RM2) =========
if(NOT TEMP_DISABLE_WIFI)
    target_compile_definitions(heatbeat PRIVATE
        PICO_CYW43_SUPPORTED=1
        CYW43_LWIP=1
        PICO_BLUETOOTH=0
        TEMP_DISABLE_WIFI=0
        HB_DEFAULT_HOST="192.168.55.119"     # <── IP  komputera
        HB_DEFAULT_PORT=8000
        HB_DEFAULT_DEVICE_ID=1

        # Domyślne mapowanie Pico W / RM2 (SP/CE): WL_ON, DAT0, CMD/CS, CLK
        CYW43_WL_GPIO_POWER=23
        CYW43_WL_GPIO_DAT0=24
        CYW43_WL_GPIO_CMD=25
        CYW43_WL_GPIO_CLK=29
    )
else()
    target_compile_definitions(heatbeat PRIVATE
        PICO_CYW43_SUPPORTED=0
        CYW43_LWIP=0
        PICO_BLUETOOTH=0
        TEMP_DISABLE_WIFI=1
    )
endif()

# Biblioteki podstawowe
target_link_libraries(heatbeat
    pico_stdlib
    hardware_spi
    hardware_i2c
    hardware_pwm
    hardware_sync
    hardware_gpio
    lvgl
    bsp
    lvgl_ui
    screen_objects
    bme280
    no-OS-FatFS-SD-SDIO-SPI-RPi-Pico
)

# Linkuj CYW43 tylko gdy WiFi jest włączone
if(NOT TEMP_DISABLE_WIFI)
    target_link_libraries(heatbeat pico_cyw43_arch_lwip_poll)
endif()

# RP2350 nie ma targetu hardware_rtc -> linkuj tylko jeśli istnieje (np. przy RP2040)
if (TARGET hardware_rtc)
    target_link_libraries(heatbeat hardware_rtc)
endif()

pico_enable_stdio_usb(heatbeat 1)
pico_enable_stdio_uart(heatbeat 0)
pico_add_extra_outputs(heatbeat)

add_subdirectory(bme280)
add_subdirectory(lvgl_ui)