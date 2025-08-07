# lvgl_ui/CMakeLists.txt

add_library(lvgl_ui
    lvgl_ui.c
    lvgl_ui.h
)

add_subdirectory(screen)

target_include_directories(lvgl_ui PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}               # lvgl_ui/
    ${CMAKE_CURRENT_SOURCE_DIR}/screen        # lvgl_ui/screen/
    ${CMAKE_CURRENT_SOURCE_DIR}/../font       # heatbeat/font/
    ${CMAKE_CURRENT_SOURCE_DIR}/../..         # heatbeat/
)

target_link_libraries(lvgl_ui
    lvgl
    screen_objects
    bme280
)