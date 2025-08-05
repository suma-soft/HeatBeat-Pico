if (TARGET tinyusb_device)
    
    add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/lvgl_ui)
    add_executable(factory
            factory.c
            ../lv_port/lv_port_disp.c
            ../lv_port/lv_port_indev.c
            ../lv_port/lv_port_fs.c
            font/lv_font_montserrat_28_pl.c
            )
    
    # pull in common dependencies
    target_link_libraries(factory 
        pico_stdlib 
        lvgl
        bsp
        lvgl_ui
        no-OS-FatFS-SD-SDIO-SPI-RPi-Pico)

    # enable usb output, disable uart output
    pico_enable_stdio_usb(factory 1)
    pico_enable_stdio_uart(factory 0)

    # create map/bin/hex/uf2 file etc.
    pico_add_extra_outputs(factory)

    # add url via pico_set_program_url
    # example_auto_set_url(factory)
elseif(PICO_ON_DEVICE)
    message("Skipping factory because TinyUSB submodule is not initialized in the SDK")
endif()
