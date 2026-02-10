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
    cal_storage.cpp
)

target_include_directories(greymatter PRIVATE ${CMAKE_SOURCE_DIR}/include)

pico_enable_stdio_usb(greymatter 1)

pico_add_extra_outputs(greymatter)

target_link_libraries(greymatter
    pico_stdlib
    hardware_spi
    hardware_gpio
    hardware_flash
    hardware_sync
)
