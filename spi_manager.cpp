#include "spi_manager.hpp"
#include "pico/stdlib.h"
#include "hardware/gpio.h"

#ifdef DEBUG_SPI_MODE
#include "debug_spi.hpp"
#endif

void SpiManager::init_gpio() {
    // Step 1: Enable level shifter FIRST (before any downstream communication)
    // TXB0106 OE is active-high
    gpio_init(HW_PINS::LEVEL_SHIFT_OE);
    gpio_set_dir(HW_PINS::LEVEL_SHIFT_OE, GPIO_OUT);
    gpio_put(HW_PINS::LEVEL_SHIFT_OE, 1);

    // Step 2: Configure IO expander reset pin
    gpio_init(HW_PINS::EXPANDER_RESET);
    gpio_set_dir(HW_PINS::EXPANDER_RESET, GPIO_OUT);
    gpio_put(HW_PINS::EXPANDER_RESET, 1);  // Start high (not in reset)

    // Step 3: Configure FAULT input pin
    gpio_init(HW_PINS::FAULT);
    gpio_set_dir(HW_PINS::FAULT, GPIO_IN);
    gpio_pull_up(HW_PINS::FAULT);  // Pull-up, FAULT is active-low

    // Step 4: Configure SPI CS pin (directly controlled, active-low)
    gpio_init(HW_PINS::SPI_CS);
    gpio_set_dir(HW_PINS::SPI_CS, GPIO_OUT);
    gpio_put(HW_PINS::SPI_CS, 1);  // Start high (deselected)
}

void SpiManager::reset_io_expanders() {
    // Pulse reset line low, then high
    gpio_put(HW_PINS::EXPANDER_RESET, 0);
    sleep_us(SPI_CONFIG::RESET_PULSE_US);
    gpio_put(HW_PINS::EXPANDER_RESET, 1);
    sleep_us(SPI_CONFIG::RESET_SETTLE_US);
}

void SpiManager::init_spi() {
#ifdef DEBUG_SPI_MODE
    // In debug mode, use bit-banged GPIO instead of hardware SPI
    g_debug_spi.init();
#else
    // Initialize SPI peripheral
    spi_inst_t* spi = SPI_CONFIG::get_spi_instance();
    spi_init(spi, SPI_CONFIG::BAUDRATE);

    // Configure SPI pins
    gpio_set_function(HW_PINS::SPI_MISO, GPIO_FUNC_SPI);
    gpio_set_function(HW_PINS::SPI_CLK, GPIO_FUNC_SPI);
    gpio_set_function(HW_PINS::SPI_MOSI, GPIO_FUNC_SPI);
    // Note: CS is GPIO-controlled, not SPI peripheral controlled

    // SPI Mode 0: CPOL=0, CPHA=0 (data sampled on rising edge)
    spi_set_format(spi,
                   8,           // 8 bits per transfer
                   SPI_CPOL_0,  // Clock polarity: idle low
                   SPI_CPHA_0,  // Clock phase: sample on rising edge
                   SPI_MSB_FIRST);
#endif
}

void SpiManager::init() {
    // Full initialization sequence per documentation:
    // 1. Set GP21 HIGH (enable TXB0106 level-shifter) - MUST BE FIRST
    // 2. Configure GP22 as output, pulse reset to IO expanders
    // 3. Configure GP20 as input (FAULT line)
    // 4. Initialize SPI peripheral on spi0
    // 5. Configure IO expanders (HAEN, IODIR, etc.)

    init_gpio();           // Steps 1-3
    init_spi();            // Step 4
    reset_io_expanders();  // Reset before configuring

    // Step 5: Initialize IO expanders
    io_expander_.init(SPI_CONFIG::get_spi_instance());

    initialized_ = true;
}

void SpiManager::select_downstream(uint8_t board_id, uint8_t device_id) {
    // Use IO expander to set CS bits and enable decoder tree
    io_expander_.set_dac_select(board_id, device_id);
}

void SpiManager::deselect() {
    // Disable decoder tree via IO expander
    io_expander_.deselect_dac();
}

void SpiManager::raw_transfer(const uint8_t* tx_data, uint8_t* rx_data, size_t len) {
    // Raw SPI transfer without CS management
    // Used for direct IO expander access (which manages its own CS)
#ifdef DEBUG_SPI_MODE
    g_debug_spi.transaction(tx_data, rx_data, len);
#else
    spi_inst_t* spi = SPI_CONFIG::get_spi_instance();
    if (rx_data != nullptr) {
        spi_write_read_blocking(spi, tx_data, rx_data, len);
    } else {
        spi_write_blocking(spi, tx_data, len);
    }
#endif
}

void SpiManager::transaction(uint8_t board_id, uint8_t device_id,
                              const uint8_t* tx_data, uint8_t* rx_data, size_t len) {
    // DAC transaction protocol:
    // 1. Write to IO expander: set CS0-CS4 bits + assert D_EN
    //    (IO expander handles its own CS via cs_assert/cs_release)
    // 2. Perform DAC SPI transaction WITHOUT asserting GP17 CS
    //    (decoder tree provides CS to selected DAC)
    // 3. Deassert D_EN via IO expander when done

    // Step 1: Select the DAC via decoder tree
    select_downstream(board_id, device_id);

    // Small delay to ensure decoder output is stable
    sleep_us(1);

    // Step 2: Perform SPI transaction to DAC
    // Note: We do NOT assert GP17 CS here - decoder tree handles CS
#ifdef DEBUG_SPI_MODE
    g_debug_spi.transaction(tx_data, rx_data, len);
#else
    spi_inst_t* spi = SPI_CONFIG::get_spi_instance();
    if (rx_data != nullptr) {
        spi_write_read_blocking(spi, tx_data, rx_data, len);
    } else {
        spi_write_blocking(spi, tx_data, len);
    }
#endif

    // Small delay for DAC to latch data
    sleep_us(1);

    // Step 3: Deselect DAC
    deselect();
}

void SpiManager::pulse_ldac() {
    io_expander_.pulse_ldac();
}

bool SpiManager::is_fault_active() {
    // FAULT line is active-low
    return !gpio_get(HW_PINS::FAULT);
}
