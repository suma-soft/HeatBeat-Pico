add_library(bme280 STATIC
    bme280.c
)

target_include_directories(bme280 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
