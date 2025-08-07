add_library(screen_objects
    main_screen.c
    drawing_screen.c
    color_screen.c
)

target_include_directories(screen_objects PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}                        # screen/
    ${CMAKE_CURRENT_SOURCE_DIR}/..                     # lvgl_ui/
    ${CMAKE_CURRENT_SOURCE_DIR}/../../                 # heatbeat/
    ${CMAKE_CURRENT_SOURCE_DIR}/../../font             # font/
    ${CMAKE_CURRENT_SOURCE_DIR}/../../bme280           # bme280/
)

target_link_libraries(screen_objects
    lvgl
)
