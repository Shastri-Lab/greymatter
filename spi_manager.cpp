#include "spi_manager.hpp"
#include "pico/stdlib.h"
#include "hardware/gpio.h"

void SpiManager::init_gpio() {
#ifdef SINGLE_BOARD_MODE
    // Single-board mode: Direct GPIO chip select, no level shifter or expanders

    // Initialize CS pins as outputs, all HIGH (deselected)
    gpio_init(HW_PINS_SINGLE::CS_DAC0);
    gpio_set_dir(HW_PINS_SINGLE::CS_DAC0, GPIO_OUT);
    gpio_put(HW_PINS_SINGLE::CS_DAC0, 1);

    gpio_init(HW_PINS_SINGLE::CS_DAC1);
    gpio_set_dir(HW_PINS_SINGLE::CS_DAC1, GPIO_OUT);
    gpio_put(HW_PINS_SINGLE::CS_DAC1, 1);

    gpio_init(HW_PINS_SINGLE::CS_DAC2);
    gpio_set_dir(HW_PINS_SINGLE::CS_DAC2, GPIO_OUT);
    gpio_put(HW_PINS_SINGLE::CS_DAC2, 1);

    // CLR pin: output, HIGH (not clearing)
    gpio_init(HW_PINS_SINGLE::CLR);
    gpio_set_dir(HW_PINS_SINGLE::CLR, GPIO_OUT);
    gpio_put(HW_PINS_SINGLE::CLR, 1);

    // FAULT pin: input with pull-up
    gpio_init(HW_PINS_SINGLE::FAULT);
    gpio_set_dir(HW_PINS_SINGLE::FAULT, GPIO_IN);
    gpio_pull_up(HW_PINS_SINGLE::FAULT);
#else
    // Multi-board mode: Level shifter, IO expanders, decoder tree

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
#endif
}

#ifndef SINGLE_BOARD_MODE
void SpiManager::reset_io_expanders() {
    // Pulse reset line low, then high
    gpio_put(HW_PINS::EXPANDER_RESET, 0);
    sleep_us(SPI_CONFIG::RESET_PULSE_US);
    gpio_put(HW_PINS::EXPANDER_RESET, 1);
    sleep_us(SPI_CONFIG::RESET_SETTLE_US);
}
#endif

void SpiManager::init_spi() {
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
}

void SpiManager::init() {
#ifdef SINGLE_BOARD_MODE
    // Single-board mode: Simple initialization, no level shifter or expanders
    init_gpio();
    init_spi();
#else
    // Multi-board mode: Full initialization sequence per documentation:
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
#endif
    initialized_ = true;
}

void SpiManager::select_downstream(uint8_t board_id, uint8_t device_id) {
#ifdef SINGLE_BOARD_MODE
    (void)board_id;  // Always 0 in single-board mode
    deselect();  // Deselect any previous DAC first

    // Map device_id to GPIO pin
    uint cs_pin;
    switch (device_id) {
        case 0: cs_pin = HW_PINS_SINGLE::CS_DAC0; break;
        case 1: cs_pin = HW_PINS_SINGLE::CS_DAC1; break;
        case 2: cs_pin = HW_PINS_SINGLE::CS_DAC2; break;
        default: return;  // Invalid device_id
    }
    gpio_put(cs_pin, 0);  // Assert CS (active low)
    current_selected_dac_ = device_id;
#else
    // Use IO expander to set CS bits and enable decoder tree
    io_expander_.set_dac_select(board_id, device_id);
#endif
}

void SpiManager::deselect() {
#ifdef SINGLE_BOARD_MODE
    // Deassert all CS pins
    if (current_selected_dac_ < 3) {
        uint cs_pin;
        switch (current_selected_dac_) {
            case 0: cs_pin = HW_PINS_SINGLE::CS_DAC0; break;
            case 1: cs_pin = HW_PINS_SINGLE::CS_DAC1; break;
            case 2: cs_pin = HW_PINS_SINGLE::CS_DAC2; break;
            default: return;
        }
        gpio_put(cs_pin, 1);  // Deassert CS
        current_selected_dac_ = 0xFF;
    }
#else
    // Disable decoder tree via IO expander
    io_expander_.deselect_dac();
#endif
}

void SpiManager::raw_transfer(const uint8_t* tx_data, uint8_t* rx_data, size_t len) {
    // Raw SPI transfer without CS management
    // Used for direct IO expander access (which manages its own CS)
    spi_inst_t* spi = SPI_CONFIG::get_spi_instance();
    if (rx_data != nullptr) {
        spi_write_read_blocking(spi, tx_data, rx_data, len);
    } else {
        spi_write_blocking(spi, tx_data, len);
    }
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
    spi_inst_t* spi = SPI_CONFIG::get_spi_instance();
    if (rx_data != nullptr) {
        spi_write_read_blocking(spi, tx_data, rx_data, len);
    } else {
        spi_write_blocking(spi, tx_data, len);
    }

    // Small delay for DAC to latch data
    sleep_us(1);

    // Step 3: Deselect DAC
    deselect();
}

void SpiManager::pulse_ldac() {
#ifndef SINGLE_BOARD_MODE
    io_expander_.pulse_ldac();
#endif
    // Single-board mode: DACs configured for immediate update, no LDAC needed
}

void SpiManager::assert_clear() {
#ifdef SINGLE_BOARD_MODE
    gpio_put(HW_PINS_SINGLE::CLR, 0);  // Assert CLR (active low)
#else
    io_expander_.assert_clear();
#endif
}

void SpiManager::release_clear() {
#ifdef SINGLE_BOARD_MODE
    gpio_put(HW_PINS_SINGLE::CLR, 1);  // Release CLR
#else
    io_expander_.release_clear();
#endif
}

bool SpiManager::is_fault_active() {
    // FAULT line is active-low
#ifdef SINGLE_BOARD_MODE
    return !gpio_get(HW_PINS_SINGLE::FAULT);
#else
    return !gpio_get(HW_PINS::FAULT);
#endif
}
