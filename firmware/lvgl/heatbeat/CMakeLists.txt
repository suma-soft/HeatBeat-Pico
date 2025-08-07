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
)

# Biblioteki powiązane
target_link_libraries(${PROJECT_NAME} 
    pico_stdlib 
    lvgl
    bsp
    lvgl_ui
    screen_objects
    bme280
    no-OS-FatFS-SD-SDIO-SPI-RPi-Pico
)

# Konfiguracja wejścia/wyjścia
pico_enable_stdio_usb(${PROJECT_NAME} 1)  # zmień z heatbeat_main
pico_enable_stdio_uart(${PROJECT_NAME} 0)  # zmień z heatbeat_main

# Eksportuj bin/uf2
pico_add_extra_outputs(${PROJECT_NAME})

add_subdirectory(bme280)
add_subdirectory(lvgl_ui)
