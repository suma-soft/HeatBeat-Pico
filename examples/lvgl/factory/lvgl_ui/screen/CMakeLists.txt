add_library(screen_objects
    main_screen.c
    main_screen.h
    drawing_screen.c
    drawing_screen.h
    color_screen.c
    color_screen.h
)

target_include_directories(screen_objects PUBLIC
    ${CMAKE_CURRENT_LIST_DIR}                             # screen/
    ${CMAKE_CURRENT_LIST_DIR}/..                          # lvgl_ui/
    ${CMAKE_CURRENT_LIST_DIR}/../../..                    # factory/
    ${CMAKE_CURRENT_LIST_DIR}/../../font                  # font/
    ${CMAKE_CURRENT_LIST_DIR}/../../bme280                 # bme280/
)

target_link_libraries(screen_objects lvgl)
