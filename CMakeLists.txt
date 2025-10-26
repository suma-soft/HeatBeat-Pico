# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(PICO_CYW43_SUPPORTED 1)
set(sdkVersion 2.1.1)
set(toolchainVersion RISCV_RPI_2_0_0_5)
set(picotoolVersion 2.1.1)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
cmake_minimum_required(VERSION 3.12)

# >>> TU ZMIANA <<<
set(PICO_BOARD pico2_w CACHE STRING "Board type")

# Pull in SDK (must be before project)
include(pico_sdk_import.cmake)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
project(heatbeat C CXX ASM)

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
