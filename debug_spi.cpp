#include "debug_spi.hpp"

#ifdef DEBUG_SPI_MODE

#include "io_expander.hpp"  // For HW_PINS namespace
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include <cstdio>

// Global instance
DebugSpi g_debug_spi;

void DebugSpi::init() {
    // Convert SPI pins from SPI function to GPIO function
    // This allows direct bit-bang control

    // MOSI (GP19) - output
    gpio_init(HW_PINS::SPI_MOSI);
    gpio_set_dir(HW_PINS::SPI_MOSI, GPIO_OUT);
    gpio_put(HW_PINS::SPI_MOSI, 0);

    // MISO (GP16) - input
    gpio_init(HW_PINS::SPI_MISO);
    gpio_set_dir(HW_PINS::SPI_MISO, GPIO_IN);
    gpio_pull_up(HW_PINS::SPI_MISO);

    // CLK (GP18) - output, idle low (Mode 0)
    gpio_init(HW_PINS::SPI_CLK);
    gpio_set_dir(HW_PINS::SPI_CLK, GPIO_OUT);
    gpio_put(HW_PINS::SPI_CLK, 0);

    // CS (GP17) - output, idle high
    gpio_init(HW_PINS::SPI_CS);
    gpio_set_dir(HW_PINS::SPI_CS, GPIO_OUT);
    gpio_put(HW_PINS::SPI_CS, 1);

    // Initialize loopback pins (GP0-GP3) as outputs
    gpio_init(DEBUG_SPI_CONFIG::LOOPBACK_MOSI);
    gpio_set_dir(DEBUG_SPI_CONFIG::LOOPBACK_MOSI, GPIO_OUT);
    gpio_put(DEBUG_SPI_CONFIG::LOOPBACK_MOSI, 0);

    gpio_init(DEBUG_SPI_CONFIG::LOOPBACK_MISO);
    gpio_set_dir(DEBUG_SPI_CONFIG::LOOPBACK_MISO, GPIO_OUT);
    gpio_put(DEBUG_SPI_CONFIG::LOOPBACK_MISO, 0);

    gpio_init(DEBUG_SPI_CONFIG::LOOPBACK_CLK);
    gpio_set_dir(DEBUG_SPI_CONFIG::LOOPBACK_CLK, GPIO_OUT);
    gpio_put(DEBUG_SPI_CONFIG::LOOPBACK_CLK, 0);

    gpio_init(DEBUG_SPI_CONFIG::LOOPBACK_CS);
    gpio_set_dir(DEBUG_SPI_CONFIG::LOOPBACK_CS, GPIO_OUT);
    gpio_put(DEBUG_SPI_CONFIG::LOOPBACK_CS, 1);

    state_ = DebugSpiState::IDLE;

    printf("[DEBUG SPI] Initialized - 1 Hz bit-banged mode\r\n");
    printf("[DEBUG SPI] Loopback pins: GP0=MOSI, GP1=MISO, GP2=CLK, GP3=CS\r\n");
}

void DebugSpi::set_trace_level(uint8_t level) {
    trace_level_ = level;
    printf("[DEBUG SPI] Trace level set to %d\r\n", level);
}

void DebugSpi::set_step_mode(bool enabled) {
    step_mode_ = enabled;
    printf("[DEBUG SPI] Step mode %s\r\n", enabled ? "ENABLED" : "DISABLED");
}

void DebugSpi::step() {
    step_pending_ = true;
}

void DebugSpi::set_loopback_enabled(bool enabled) {
    loopback_enabled_ = enabled;
    printf("[DEBUG SPI] Loopback %s\r\n", enabled ? "ENABLED" : "DISABLED");
}

void DebugSpi::set_mosi(bool value) {
    gpio_put(HW_PINS::SPI_MOSI, value);
}

bool DebugSpi::read_miso() {
    return gpio_get(HW_PINS::SPI_MISO);
}

void DebugSpi::set_clk(bool value) {
    gpio_put(HW_PINS::SPI_CLK, value);
}

void DebugSpi::set_cs(bool value) {
    gpio_put(HW_PINS::SPI_CS, value);
}

void DebugSpi::update_loopback() {
    if (!loopback_enabled_) return;

    // Mirror SPI pins to loopback pins
    gpio_put(DEBUG_SPI_CONFIG::LOOPBACK_MOSI, gpio_get(HW_PINS::SPI_MOSI));
    gpio_put(DEBUG_SPI_CONFIG::LOOPBACK_MISO, gpio_get(HW_PINS::SPI_MISO));
    gpio_put(DEBUG_SPI_CONFIG::LOOPBACK_CLK, gpio_get(HW_PINS::SPI_CLK));
    gpio_put(DEBUG_SPI_CONFIG::LOOPBACK_CS, gpio_get(HW_PINS::SPI_CS));
}

void DebugSpi::trace_edge(const char* edge, bool mosi, bool miso, bool clk, bool cs) {
    if (trace_level_ >= DEBUG_SPI_CONFIG::TRACE_EDGE) {
        printf("[SPI %s] CS=%d CLK=%d MOSI=%d MISO=%d\r\n",
               edge, cs ? 1 : 0, clk ? 1 : 0, mosi ? 1 : 0, miso ? 1 : 0);
    }
}

void DebugSpi::trace_bit(uint8_t bit_num, bool mosi_out, bool miso_in, bool clk, bool cs) {
    if (trace_level_ >= DEBUG_SPI_CONFIG::TRACE_BIT) {
        printf("[SPI BIT %d] MOSI=%d -> MISO=%d (CLK=%d CS=%d)\r\n",
               bit_num, mosi_out ? 1 : 0, miso_in ? 1 : 0, clk ? 1 : 0, cs ? 1 : 0);
    }
}

void DebugSpi::trace_byte_summary(uint8_t tx_byte, uint8_t rx_byte) {
    if (trace_level_ >= DEBUG_SPI_CONFIG::TRACE_SUMMARY) {
        printf("[SPI BYTE] TX=0x%02X RX=0x%02X\r\n", tx_byte, rx_byte);
    }
}

void DebugSpi::wait_for_step() {
    if (!step_mode_) return;

    printf("[DEBUG SPI] Waiting for STEP command...\r\n");
    state_ = DebugSpiState::WAITING_FOR_STEP;
    step_pending_ = false;

    // Block until step() is called (from main loop processing DEBUG:STEP command)
    while (!step_pending_) {
        sleep_ms(10);
        tight_loop_contents();
    }

    step_pending_ = false;
    state_ = DebugSpiState::IDLE;
}

void DebugSpi::cs_assert() {
    state_ = DebugSpiState::CS_ASSERT;
    set_cs(false);
    update_loopback();
    trace_edge("CS_LOW ", gpio_get(HW_PINS::SPI_MOSI), read_miso(), false, false);
    sleep_ms(DEBUG_SPI_CONFIG::HALF_PERIOD_MS);
}

void DebugSpi::cs_release() {
    state_ = DebugSpiState::CS_RELEASE;
    set_cs(true);
    update_loopback();
    trace_edge("CS_HIGH", gpio_get(HW_PINS::SPI_MOSI), read_miso(), false, true);
    sleep_ms(DEBUG_SPI_CONFIG::HALF_PERIOD_MS);
    state_ = DebugSpiState::IDLE;
}

void DebugSpi::transaction(const uint8_t* tx_data, uint8_t* rx_data, size_t len) {
    printf("\r\n[DEBUG SPI] Starting transaction: %zu bytes\r\n", len);
    printf("[DEBUG SPI] TX data: ");
    for (size_t i = 0; i < len; i++) {
        printf("0x%02X ", tx_data[i]);
    }
    printf("\r\n");

    for (size_t byte_idx = 0; byte_idx < len; byte_idx++) {
        uint8_t tx_byte = tx_data[byte_idx];
        uint8_t rx_byte = 0;

        printf("[DEBUG SPI] Byte %zu: TX=0x%02X\r\n", byte_idx, tx_byte);

        // SPI Mode 0: CPOL=0, CPHA=0
        // Data is set on falling edge, sampled on rising edge
        // Clock idles low

        for (int bit = 7; bit >= 0; bit--) {
            wait_for_step();

            // Set MOSI (data changes while CLK is low)
            bool mosi_bit = (tx_byte >> bit) & 1;
            set_mosi(mosi_bit);
            update_loopback();

            state_ = DebugSpiState::CLK_LOW;
            trace_edge("CLK_LOW ", mosi_bit, read_miso(), false, false);
            sleep_ms(DEBUG_SPI_CONFIG::HALF_PERIOD_MS);

            wait_for_step();

            // Rising edge - sample MISO
            set_clk(true);
            update_loopback();

            bool miso_bit = read_miso();
            rx_byte |= (miso_bit ? 1 : 0) << bit;

            state_ = DebugSpiState::CLK_HIGH;
            trace_edge("CLK_HIGH", mosi_bit, miso_bit, true, false);
            trace_bit(7 - bit, mosi_bit, miso_bit, true, false);
            sleep_ms(DEBUG_SPI_CONFIG::HALF_PERIOD_MS);

            wait_for_step();

            // Falling edge
            set_clk(false);
            update_loopback();
        }

        if (rx_data != nullptr) {
            rx_data[byte_idx] = rx_byte;
        }

        trace_byte_summary(tx_byte, rx_byte);
    }

    printf("[DEBUG SPI] Transaction complete\r\n");
    if (rx_data != nullptr) {
        printf("[DEBUG SPI] RX data: ");
        for (size_t i = 0; i < len; i++) {
            printf("0x%02X ", rx_data[i]);
        }
        printf("\r\n");
    }

    state_ = DebugSpiState::IDLE;
}

#endif // DEBUG_SPI_MODE
