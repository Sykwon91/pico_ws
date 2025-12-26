cmake_minimum_required(VERSION 3.13)

# initialize the SDK based on PICO_SDK_PATH
# note: this must happen before project()
set(PICO_BOARD pico_w)
set(PICO_SDK_PATH $ENV{PICO_SDK_PATH})

include(${PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(pico_buzzer_pwm)

# initialize the Raspberry Pi Pico SDK
pico_sdk_init()

# rest of your project
add_executable(pico_buzzer_pwm
    src/main.cpp
)

# Add pico_stdlib library which aggregates commonly used features
target_link_libraries(pico_buzzer_pwm pico_stdlib hardware_pwm )

pico_enable_stdio_usb(pico_buzzer_pwm 1)
pico_enable_stdio_uart(pico_buzzer_pwm 0)

# create map/bin/hex/uf2 file in addition to ELF.
pico_add_extra_outputs(pico_buzzer_pwm)

