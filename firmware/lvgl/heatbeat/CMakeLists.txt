cmake_minimum_required(VERSION 3.13)

project(heatbeat C)

add_executable(${PROJECT_NAME}
    main.c
    bme280_port.c
    ../lv_port/lv_port_disp.c
    ../lv_port/lv_port_indev.c
    ../lv_port/lv_port_fs.c
    font/lv_font_montserrat_28_pl.c
)

target_include_directories(${PROJECT_NAME} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/bme280
    ${CMAKE_CURRENT_SOURCE_DIR}/font
    ${CMAKE_CURRENT_SOURCE_DIR}/lvgl_ui
    ${CMAKE_CURRENT_SOURCE_DIR}/lvgl_ui/screen
    ${CMAKE_CURRENT_SOURCE_DIR}/net
)

# --- Konfiguracja użytkownika ---
set(WIFI_SSID "KAMNET_8960" CACHE STRING "WiFi SSID")
set(WIFI_PASS "Pawianywchodzanasciany" CACHE STRING "WiFi password")
option(ENABLE_HTTP_CLIENT "Enable tiny HTTP client" OFF)
option(ENABLE_WIFI "Enable WiFi (CYW43 + lwIP)" ON)

# Definicje kompilacji
target_compile_definitions(${PROJECT_NAME} PRIVATE
    WIFI_SSID="${WIFI_SSID}"
    WIFI_PASS="${WIFI_PASS}"
    ENABLE_HTTP_CLIENT=$<IF:$<BOOL:${ENABLE_HTTP_CLIENT}>,1,0>
    ENABLE_WIFI=$<IF:$<BOOL:${ENABLE_WIFI}>,1,0>

    # Stos CYW43 + lwIP (arch = poll, bez RTOS)
    PICO_CYW43_SUPPORTED=1
    CYW43_LWIP=1
    PICO_CYW43_ARCH_POLL=1
    PICO_BLUETOOTH=0

    # UWAGA: na Pico W / Pico 2 W NIE definiujemy własnych pinów CYW43_*
    # SDK ma właściwy port i NVRAM. Własne makra potrafią zabić start.
)

# Biblioteki – kolejność ma znaczenie
target_link_libraries(${PROJECT_NAME}
    pico_stdlib
    lvgl
    bsp
    lvgl_ui
    screen_objects
    bme280
    pico_cyw43_arch_lwip_poll
    pico_lwip_nosys
    # HTTPS? odkomentuj:
    # pico_lwip_mbedtls
    # mbedtls
    no-OS-FatFS-SD-SDIO-SPI-RPi-Pico
)

# STDIO
pico_enable_stdio_usb(${PROJECT_NAME} 1)
pico_enable_stdio_uart(${PROJECT_NAME} 0)

# Artefakty (elf/bin/uf2)
pico_add_extra_outputs(${PROJECT_NAME})

# Podmoduły lokalne
add_subdirectory(bme280)
add_subdirectory(lvgl_ui)
