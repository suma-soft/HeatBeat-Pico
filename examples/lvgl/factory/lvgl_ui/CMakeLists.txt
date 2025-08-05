# Dodaj bibliotekę lvgl_ui z plikami źródłowymi
add_library(lvgl_ui
    screen/main_screen.c
    lvgl_ui.c 
)

# Ustaw katalogi zawierające nagłówki (header files)
target_include_directories(lvgl_ui PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/screen
    ${CMAKE_CURRENT_SOURCE_DIR}/../font
)

# Połącz z bibliotekami, jeśli to konieczne (opcjonalnie rozszerz)
target_link_libraries(lvgl_ui
    lvgl
)
