if (TARGET tinyusb_device)

    # Dodaj podkatalog z biblioteką BME280 (własna kopia drivera Bosh)
    add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/bme280)

    # Dodaj GUI z ekranami
    add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/lvgl_ui)

    # Główna aplikacja
    add_executable(factory
        factory.c
        bme280_port.c
        ../lv_port/lv_port_disp.c
        ../lv_port/lv_port_indev.c
        ../lv_port/lv_port_fs.c
        font/lv_font_montserrat_28_pl.c
    )

    # Ścieżki do nagłówków
    target_include_directories(factory PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/bme280
        ${CMAKE_CURRENT_SOURCE_DIR}/font
        ${CMAKE_CURRENT_SOURCE_DIR}/lvgl_ui
        ${CMAKE_CURRENT_SOURCE_DIR}/lvgl_ui/screen
    )

    # Biblioteki powiązane
    target_link_libraries(factory 
        pico_stdlib 
        lvgl
        bsp
        lvgl_ui
        screen_objects
        bme280
        no-OS-FatFS-SD-SDIO-SPI-RPi-Pico
    )

    # Konfiguracja wejścia/wyjścia
    pico_enable_stdio_usb(factory 1)
    pico_enable_stdio_uart(factory 0)

    # Eksportuj bin/uf2
    pico_add_extra_outputs(factory)

else()
    message("Skipping factory because TinyUSB submodule is not initialized in the SDK")
endif()
