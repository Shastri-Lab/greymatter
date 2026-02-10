cmake_minimum_required(VERSION 3.13)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(pico_sdk_import.cmake)

project(greymatter C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

# SPI Baud Rate - configurable for debugging with logic analyzers
# Default: 10 MHz (conservative). DACs support up to 50 MHz.
# For logic analyzer debugging, use lower values (e.g., 1000000 for 1 MHz)
set(SPI_BAUDRATE "10000000" CACHE STRING "SPI clock rate in Hz (default 10 MHz)")
add_compile_definitions(SPI_BAUDRATE=${SPI_BAUDRATE})
message(STATUS "SPI baud rate: ${SPI_BAUDRATE} Hz")

# Single-board Mode - simplified interface with direct GPIO chip select
option(SINGLE_BOARD_MODE "Single-board mode with direct GPIO chip select" OFF)
if(SINGLE_BOARD_MODE)
    add_compile_definitions(SINGLE_BOARD_MODE=1)
    message(STATUS "Single-board mode ENABLED - 1 board, 3 DACs, direct GPIO CS")
else()
    message(STATUS "Single-board mode disabled - 8 boards, IO expander CS routing")
endif()

add_subdirectory(src)
