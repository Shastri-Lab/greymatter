#ifndef SPI_MANAGER_HPP
#define SPI_MANAGER_HPP

#include <cstdint>
#include <cstddef>

#include "hardware/spi.h"
#include "io_expander.hpp"  // HW_PINS (multi-board) or HW_PINS_SINGLE (single-board)

// SPI Configuration Constants
namespace SPI_CONFIG {
    // SPI peripheral instance (spi0 is a macro, not constexpr-compatible)
    inline spi_inst_t* get_spi_instance() { return spi0; }

    // SPI baudrate (10 MHz - conservative, max is 50 MHz for DACs)
    constexpr uint32_t BAUDRATE = 10 * 1000 * 1000;

    // GPIO pins (from HW_PINS namespace in io_expander.hpp)
    // SPI_MISO = GP16, SPI_CS = GP17, SPI_CLK = GP18, SPI_MOSI = GP19

    // Timing constants
    constexpr uint32_t RESET_PULSE_US = 10;    // IO expander reset pulse duration
    constexpr uint32_t RESET_SETTLE_US = 100;  // Settle time after reset release
}

// Low-level SPI + hardware CS orchestration
// Manages SPI peripheral and uses IoExpander for DAC chip select routing
class SpiManager {
public:
    // Initialize SPI peripheral, GPIO pins, and IO expanders
    // This is the main hardware initialization entry point
    void init();

    // Perform SPI transaction to a specific DAC
    // board_id: 0-7, device_id: 0-2 (2x LTC2662 + 1x LTC2664 per board)
    // For DAC transactions, the decoder tree handles CS via IO expander
    void transaction(uint8_t board_id, uint8_t device_id,
                     const uint8_t* tx_data, uint8_t* rx_data, size_t len);

    // Perform raw SPI transaction to IO expander (CS managed by caller via io_expander)
    // This is used internally and for direct IO expander access
    void raw_transfer(const uint8_t* tx_data, uint8_t* rx_data, size_t len);

    // Pulse LDAC to update all DAC outputs
    void pulse_ldac();

    // Assert/release CLR line (clear all DACs)
    void assert_clear();
    void release_clear();

#ifndef SINGLE_BOARD_MODE
    // Access to IO expander for fault monitoring, etc. (multi-board only)
    IoExpander& io_expander() { return io_expander_; }
#endif

    // Check if FAULT line is asserted (active low)
    bool is_fault_active();

private:
#ifdef SINGLE_BOARD_MODE
    uint8_t current_selected_dac_ = 0xFF;  // 0xFF = none selected
#else
    IoExpander io_expander_;
#endif
    bool initialized_ = false;

    // Internal helpers
    void init_gpio();          // Configure GPIO pins
#ifndef SINGLE_BOARD_MODE
    void reset_io_expanders(); // Pulse reset line (multi-board only)
#endif
    void init_spi();           // Configure SPI peripheral

    void select_downstream(uint8_t board_id, uint8_t device_id);
    void deselect();
};

#endif // SPI_MANAGER_HPP
