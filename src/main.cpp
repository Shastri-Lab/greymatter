// main loop: initialize all peripherals, enter command parsing loop
#include <stdio.h>
#include <cstring>
#include <string>
#include "pico/stdlib.h"
#include "hardware/spi.h"
#include "tusb.h" // tinyusb, for usb serial

#include "scpi_parser.hpp"
#include "board_manager.hpp"
#include "spi_manager.hpp"

// Line buffer for serial input
static constexpr size_t LINE_BUFFER_SIZE = 256;
static char line_buffer[LINE_BUFFER_SIZE];
static size_t line_pos = 0;

// Global instances
static SpiManager spi_manager;
static ScpiParser parser;

// Read a line from USB serial (non-blocking)
// Returns true if a complete line was read
static bool read_line() {
    while (true) {
        int c = getchar_timeout_us(0);
        if (c == PICO_ERROR_TIMEOUT) {
            return false;
        }

        if (c == '\r' || c == '\n') {
            if (line_pos > 0) {
                line_buffer[line_pos] = '\0';
                line_pos = 0;
                return true;
            }
            // Skip empty lines
            continue;
        }

        if (c == '\b' || c == 127) {
            // Handle backspace
            if (line_pos > 0) {
                line_pos--;
                printf("\b \b");  // Erase character on terminal
            }
            continue;
        }

        // Only accept printable ASCII characters
        if (c >= 0x20 && c <= 0x7E && line_pos < LINE_BUFFER_SIZE - 1) {
            line_buffer[line_pos++] = static_cast<char>(c);
            putchar(c);  // Echo character
        }
    }
}

int main() {
    // Initialize USB stdio
    stdio_init_all();

    // Wait for USB connection (optional, helps with debugging)
    while (!tud_cdc_connected()) {
        sleep_ms(100);
    }
    sleep_ms(100);  // Extra settle time

    // Print startup banner
    printf("\r\n");
    printf("greymatter DAC Controller v0.1\r\n");
#ifdef SINGLE_BOARD_MODE
    printf("Mode: Single-board (1 board, 3 DACs, direct GPIO CS)\r\n");
#else
    printf("Mode: Multi-board (8 boards, 24 DACs, IO expander CS)\r\n");
#endif
    printf("SPI clock: %lu Hz\r\n", (unsigned long)SPI_CONFIG::BAUDRATE);
    printf("Initializing...\r\n");

    // Initialize SPI manager (includes GPIO, SPI peripheral, IO expanders)
    spi_manager.init();
    printf("SPI and IO expanders initialized.\r\n");

    // Initialize board manager with all DACs
    BoardManager board_manager(spi_manager);
    board_manager.init_all();
    printf("All DACs initialized.\r\n");

    // Check for any initial faults
    if (spi_manager.is_fault_active()) {
        printf("WARNING: FAULT line is active!\r\n");
#ifndef SINGLE_BOARD_MODE
        uint32_t faults = spi_manager.io_expander().read_faults();
        printf("Fault mask: 0x%06lX\r\n", faults);
#else
        printf("(Cannot identify which DAC in single-board mode)\r\n");
#endif
    } else {
        printf("No faults detected.\r\n");
    }

    // Flush any garbage from USB buffer before accepting commands
    while (getchar_timeout_us(0) != PICO_ERROR_TIMEOUT) {}

    printf("Ready. Enter SCPI commands:\r\n");
    printf("> ");

    // Main command loop
    while (true) {
        if (read_line()) {
            printf("\r\n");

            // Parse and execute SCPI command
            ScpiCommand cmd = parser.parse(line_buffer);
            std::string response = board_manager.execute(cmd);

            // Print response
            printf("%s\r\n", response.c_str());
            printf("> ");
        }

        // Brief yield to allow USB processing
        tud_task();
    }

    return 0;
}