# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(PICO_CYW43_SUPPORTED 1)
set(sdkVersion 2.2.0)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.2.0-a4)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
cmake_minimum_required(VERSION 3.13)

# ✅ WYMUŚ ARM (usuń RISC-V)
set(PICO_PLATFORM rp2350-arm-s CACHE STRING "Pico platform" FORCE)
set(PICO_BOARD pico2_w CACHE STRING "Board type")

# ✅✅ WAŻNE: Ustaw ścieżkę do lwipopts.h PRZED pico_sdk_import
set(PICO_LWIP_CUSTOM_LWIPOPTS_INCLUDE_PATH 
    "${CMAKE_SOURCE_DIR}/firmware/lvgl/heatbeat/lwipopts.h" 
    CACHE INTERNAL "Path to custom lwipopts.h")

include(pico_sdk_import.cmake)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Set the toolchain for ARM cross-compilation
set(CMAKE_C_COMPILER arm-none-eabi-gcc)
set(CMAKE_CXX_COMPILER arm-none-eabi-g++)
set(CMAKE_ASM_COMPILER arm-none-eabi-gcc)

project(HeatBeat-Pico C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

if (PICO_SDK_VERSION_STRING VERSION_LESS "2.1.0")
    message(FATAL_ERROR "Raspberry Pi Pico SDK version 2.1.0 (or later) required. Your version is ${PICO_SDK_VERSION_STRING}")
endif()

pico_sdk_init()

add_compile_options(
        -Wall
        -Wno-format
        -Wno-unused-function
)
if (CMAKE_C_COMPILER_ID STREQUAL "GNU")
    add_compile_options(-Wno-maybe-uninitialized)
endif()

add_subdirectory(libraries)
add_subdirectory(firmware)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/font/lv_font_montserrat_28_pl")
    add_subdirectory(font/lv_font_montserrat_28_pl)
endif()