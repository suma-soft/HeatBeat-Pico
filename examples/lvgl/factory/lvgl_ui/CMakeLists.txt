# Dodaj bibliotekę lvgl_ui z plikami źródłowymi
add_library(lvgl_ui
    screen/main_screen.c
    lvgl_ui.c
    lvgl_ui.h 
)

# Dodaj podfolder z ekranami
add_subdirectory(screen)

# Ścieżki do nagłówków
target_include_directories(lvgl_ui PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}           # lvgl_ui/
    ${CMAKE_CURRENT_SOURCE_DIR}/screen    # lvgl_ui/screen/
    ${CMAKE_CURRENT_SOURCE_DIR}/../font   # factory/font/
    ${CMAKE_CURRENT_SOURCE_DIR}/../..     # factory/
)

# Powiązane biblioteki
target_link_libraries(lvgl_ui
    lvgl
    bme280
    screen_objects
)
