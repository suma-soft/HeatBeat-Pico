cmake_minimum_required(VERSION 3.13)

project(heatbeat C)

add_executable(${PROJECT_NAME}
    main.c
    bme280_port.c
    ../lv_port/lv_port_disp.c
    ../lv_port/lv_port_indev.c
    ../lv_port/lv_port_fs.c
    font/lv_font_montserrat_28_pl.c
)

target_include_directories(${PROJECT_NAME} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/bme280
    ${CMAKE_CURRENT_SOURCE_DIR}/font
    ${CMAKE_CURRENT_SOURCE_DIR}/lvgl_ui
    ${CMAKE_CURRENT_SOURCE_DIR}/lvgl_ui/screen
    ${CMAKE_CURRENT_SOURCE_DIR}/net
)

# --- Konfiguracja użytkownika ---
set(WIFI_SSID "KAMNET_8960" CACHE STRING "WiFi SSID")
set(WIFI_PASS "Pawianywchodzanasciany" CACHE STRING "WiFi password")
option(ENABLE_HTTP_CLIENT "Enable tiny HTTP client" ON)
option(ENABLE_WIFI "Enable WiFi (CYW43 + lwIP)" ON)

# HeatBeat API Backend
set(HEATBEAT_API_BASE "http://192.168.55.120:8000" CACHE STRING "Backend API base URL")
set(HEATBEAT_DEVICE_ID "1" CACHE STRING "Device ID")
set(HEATBEAT_JWT_TOKEN "" CACHE STRING "JWT token (optional)")
set(HEATBEAT_JWT_EMAIL "admin@example.com" CACHE STRING "Email for JWT login")
set(HEATBEAT_JWT_PASS "admin123" CACHE STRING "Password for JWT login")

# Definicje kompilacji
target_compile_definitions(${PROJECT_NAME} PRIVATE
    WIFI_SSID="${WIFI_SSID}"
    WIFI_PASS="${WIFI_PASS}"
    ENABLE_HTTP_CLIENT=$<IF:$<BOOL:${ENABLE_HTTP_CLIENT}>,1,0>

    HEATBEAT_API_BASE="${HEATBEAT_API_BASE}"
    HEATBEAT_DEVICE_ID=${HEATBEAT_DEVICE_ID}
    HEATBEAT_JWT_TOKEN="${HEATBEAT_JWT_TOKEN}"
    HEATBEAT_JWT_EMAIL="${HEATBEAT_JWT_EMAIL}"
    HEATBEAT_JWT_PASS="${HEATBEAT_JWT_PASS}"

    # Stos CYW43 + lwIP (threadsafe_background - WYMAGA NETCONN)
    PICO_CYW43_SUPPORTED=1
    CYW43_LWIP=1
    PICO_CYW43_ARCH_THREADSAFE_BACKGROUND=1
    PICO_BLUETOOTH=0

    # lwIP: włącz netconn + socket API
    LWIP_NETCONN=1
    LWIP_SOCKET=1
    LWIP_TCP=1
    LWIP_IPV4=1
    LWIP_DNS=0  # wyłączone, używamy IP literalnych

    # Zwiększone limity (netconn wymaga więcej zasobów)
    MEMP_NUM_NETCONN=8
    DEFAULT_TCP_RECVMBOX_SIZE=8
    TCPIP_MBOX_SIZE=8
)

# Biblioteki – kolejność ma znaczenie
target_link_libraries(${PROJECT_NAME}
    pico_stdlib
    lvgl
    bsp
    lvgl_ui
    screen_objects
    bme280
    pico_cyw43_arch_lwip_threadsafe_background  # ZMIANA!
    # HTTPS? odkomentuj:
    # pico_lwip_mbedtls
    # mbedtls
    no-OS-FatFS-SD-SDIO-SPI-RPi-Pico
)

# STDIO
pico_enable_stdio_usb(${PROJECT_NAME} 1)
pico_enable_stdio_uart(${PROJECT_NAME} 0)

# Artefakty (elf/bin/uf2)
pico_add_extra_outputs(${PROJECT_NAME})

# Podmoduły lokalne
add_subdirectory(bme280)
add_subdirectory(lvgl_ui)