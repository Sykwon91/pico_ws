cmake_minimum_required(VERSION 3.13)

set(PICO_SDK_PATH $ENV{PICO_SDK_PATH})
include(${PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(rf_tester C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

add_executable(rf_tester
    src/main.cpp
    src/cc1101.cpp
    src/pulse_capture.cpp
    src/rf_test.cpp
)

target_include_directories(rf_tester PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/src
)

target_link_libraries(rf_tester
    pico_stdlib
    hardware_spi
    hardware_gpio
    hardware_timer
    hardware_sync
)

pico_enable_stdio_usb(rf_tester 1)
pico_enable_stdio_uart(rf_tester 0)

pico_add_extra_outputs(rf_tester)