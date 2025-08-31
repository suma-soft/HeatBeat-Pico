cmake_minimum_required(VERSION 3.13)

# Ustaw nazwę projektu
project(heatbeat)

add_executable(${PROJECT_NAME}  # nowa nazwa
    main.c
    bme280_port.c
    ../lv_port/lv_port_disp.c
    ../lv_port/lv_port_indev.c
    ../lv_port/lv_port_fs.c
    font/lv_font_montserrat_28_pl.c
)

# Ścieżki do nagłówków
target_include_directories(${PROJECT_NAME} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/bme280
    ${CMAKE_CURRENT_SOURCE_DIR}/font
    ${CMAKE_CURRENT_SOURCE_DIR}/lvgl_ui
    ${CMAKE_CURRENT_SOURCE_DIR}/lvgl_ui/screen
    ${CMAKE_CURRENT_SOURCE_DIR}/net
)

# Biblioteki powiązane
target_link_libraries(${PROJECT_NAME} 
    pico_stdlib 
    lvgl
    bsp
    lvgl_ui
    screen_objects
    bme280
    #pico_cyw43_arch_lwip_threadsafe_background
    pico_cyw43_arch_lwip_poll
    pico_lwip_nosys
    # odkomentuj jeżeli używasz HTTPS:
    # pico_lwip_mbedtls
    # mbedtls
    no-OS-FatFS-SD-SDIO-SPI-RPi-Pico
)

# Konfiguracja wejścia/wyjścia
pico_enable_stdio_usb(${PROJECT_NAME} 1)  # zmień z heatbeat_main
pico_enable_stdio_uart(${PROJECT_NAME} 0)  # zmień z heatbeat_main

# Eksportuj bin/uf2
pico_add_extra_outputs(${PROJECT_NAME})

# --- Parametry przekazywane z linii cmake -D... (żeby nie było "not used") ---
set(WIFI_SSID "KAMNET_8960" CACHE STRING "WiFi SSID")
set(WIFI_PASS "Pawianywchodzanasciany" CACHE STRING "WiFi password")
option(ENABLE_HTTP_CLIENT "Enable tiny HTTP client" OFF)

target_compile_definitions(heatbeat PRIVATE
  WIFI_SSID="${WIFI_SSID}"
  WIFI_PASS="${WIFI_PASS}"
  ENABLE_HTTP_CLIENT=$<IF:$<BOOL:${ENABLE_HTTP_CLIENT}>,1,0>
)

add_subdirectory(bme280)
add_subdirectory(lvgl_ui)
