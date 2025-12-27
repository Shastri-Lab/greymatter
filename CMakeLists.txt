cmake_minimum_required(VERSION 3.13)
include($ENV{PICO_SDK_PATH}/pico_sdk_init.cmake)

project(greymatter C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
pico_sdk_init()

# Debug SPI Mode - enables 1 Hz bit-banged SPI for LED debugging
option(DEBUG_SPI_MODE "Enable 1 Hz bit-banged SPI for LED debugging" OFF)
if(DEBUG_SPI_MODE)
    add_compile_definitions(DEBUG_SPI_MODE=1)
    message(STATUS "Debug SPI mode ENABLED - 1 Hz bit-banged SPI")
else()
    message(STATUS "Debug SPI mode disabled - normal 10 MHz hardware SPI")
endif()

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
    debug_spi.cpp
)

pico_enable_stdio_usb(greymatter 1)

pico_add_extra_outputs(greymatter)
target_link_libraries(greymatter
    pico_stdlib
    hardware_spi
    hardware_gpio
)
