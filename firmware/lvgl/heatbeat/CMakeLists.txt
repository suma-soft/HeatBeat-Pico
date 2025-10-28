add_executable(heatbeat
    main.c
    bme280_port.c
    ../lv_port/lv_port_disp.c
    ../lv_port/lv_port_indev.c
    ../lv_port/lv_port_fs.c
    font/lv_font_montserrat_28_pl.c
)

target_include_directories(heatbeat PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/bme280
    ${CMAKE_CURRENT_SOURCE_DIR}/font
    ${CMAKE_CURRENT_SOURCE_DIR}/lvgl_ui
    ${CMAKE_CURRENT_SOURCE_DIR}/lvgl_ui/screen
)

set(WIFI_SSID "KAMNET_8960" CACHE STRING "WiFi SSID")
set(WIFI_PASS "Pawianywchodzanasciany" CACHE STRING "WiFi password")
option(ENABLE_HTTP_CLIENT "Enable tiny HTTP client" ON)
set(HEATBEAT_API_HOST "192.168.55.120" CACHE STRING "Backend API host IP")
set(HEATBEAT_API_PORT "8000" CACHE STRING "Backend API port")

target_compile_definitions(heatbeat PRIVATE
    WIFI_SSID="${WIFI_SSID}"
    WIFI_PASS="${WIFI_PASS}"
    ENABLE_HTTP_CLIENT=$<IF:$<BOOL:${ENABLE_HTTP_CLIENT}>,1,0>
    HEATBEAT_API_HOST="${HEATBEAT_API_HOST}"
    HEATBEAT_API_PORT=${HEATBEAT_API_PORT}
    PICO_CYW43_SUPPORTED=1
    CYW43_LWIP=1
    PICO_BLUETOOTH=0
)

target_link_libraries(heatbeat
    pico_stdlib
    lvgl
    bsp
    lvgl_ui
    screen_objects
    bme280
    pico_cyw43_arch_lwip_threadsafe_background
    no-OS-FatFS-SD-SDIO-SPI-RPi-Pico
)

pico_enable_stdio_usb(heatbeat 1)
pico_enable_stdio_uart(heatbeat 0)
pico_add_extra_outputs(heatbeat)

add_subdirectory(bme280)
add_subdirectory(lvgl_ui)