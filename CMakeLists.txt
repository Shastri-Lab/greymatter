cmake_minimum_required(VERSION 3.13)
include($ENV{PICO_SDK_PATH}/pico_sdk_init.cmake)

project(greymatter C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
pico_sdk_init()


add_executable(greymatter
    main.cpp
    utils.cpp
    io_expander.cpp
    spi_manager.cpp
    dac_device.cpp
    ltc2662.cpp
    ltc2664.cpp
    board_manager.cpp
    scpi_parser.cpp
)

pico_enable_stdio_usb(greymatter 1)

pico_add_extra_outputs(greymatter)
target_link_libraries(greymatter
    pico_stdlib
    hardware_spi
    hardware_gpio
)
