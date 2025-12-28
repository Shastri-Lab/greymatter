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

        if (line_pos < LINE_BUFFER_SIZE - 1) {
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
    printf("GreyMatter DAC Controller v0.1\r\n");
    printf("8 boards x 3 DACs (2x LTC2662 + 1x LTC2664)\r\n");
#ifdef DEBUG_SPI_MODE
    printf("*** DEBUG MODE ENABLED ***\r\n");
    printf("SPI: 1 Hz bit-banged for LED visibility\r\n");
    printf("Loopback pins: GP0=MOSI, GP1=MISO, GP2=CLK, GP3=CS\r\n");
    printf("Commands: DEBUG:TRACE, DEBUG:STEP:MODE, DEBUG:STEP, DEBUG:STATUS?\r\n");
    printf("          DEBUG:TEST:BYTE <hex>, DEBUG:TEST:EXPANDER <addr>\r\n");
#endif
    printf("Initializing...\r\n");

    // Initialize SPI manager (includes GPIO, SPI peripheral, IO expanders)
    spi_manager.init();
    printf("SPI and IO expanders initialized.\r\n");

    // Initialize board manager with all DACs
    BoardManager board_manager(spi_manager);
#ifndef DEBUG_SPI_MODE
    board_manager.init_all();
    printf("All DACs initialized.\r\n");

    // Check for any initial faults
    if (spi_manager.is_fault_active()) {
        printf("WARNING: FAULT line is active!\r\n");
        uint32_t faults = spi_manager.io_expander().read_faults();
        printf("Fault mask: 0x%06lX\r\n", faults);
    } else {
        printf("No faults detected.\r\n");
    }
#endif

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