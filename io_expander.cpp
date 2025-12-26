#include "io_expander.hpp"
#include "hardware/gpio.h"

void IoExpander::cs_assert() {
    gpio_put(HW_PINS::SPI_CS, 0);
}

void IoExpander::cs_release() {
    gpio_put(HW_PINS::SPI_CS, 1);
}

void IoExpander::write_register(uint8_t hw_addr, uint8_t reg, uint8_t value) {
    uint8_t tx_buf[3] = {
        MCP23S17::write_opcode(hw_addr),
        reg,
        value
    };

    cs_assert();
    spi_write_blocking(spi_, tx_buf, 3);
    cs_release();
}

uint8_t IoExpander::read_register(uint8_t hw_addr, uint8_t reg) {
    uint8_t tx_buf[3] = {
        MCP23S17::read_opcode(hw_addr),
        reg,
        0x00  // Dummy byte to clock out data
    };
    uint8_t rx_buf[3] = {0};

    cs_assert();
    spi_write_read_blocking(spi_, tx_buf, rx_buf, 3);
    cs_release();

    return rx_buf[2];  // Data is in third byte
}

void IoExpander::write_gpio16(uint8_t hw_addr, uint16_t value) {
    uint8_t tx_buf[4] = {
        MCP23S17::write_opcode(hw_addr),
        MCP23S17::REG_GPIOA,
        static_cast<uint8_t>(value & 0xFF),         // GPIOA (low byte)
        static_cast<uint8_t>((value >> 8) & 0xFF)   // GPIOB (high byte, auto-increment)
    };

    cs_assert();
    spi_write_blocking(spi_, tx_buf, 4);
    cs_release();

    // Update cache
    if (hw_addr < EXPANDER_ADDR::NUM_EXPANDERS) {
        expander_cache_[hw_addr] = value;
    }
}

uint16_t IoExpander::read_gpio16(uint8_t hw_addr) {
    uint8_t tx_buf[4] = {
        MCP23S17::read_opcode(hw_addr),
        MCP23S17::REG_GPIOA,
        0x00,  // Dummy for GPIOA
        0x00   // Dummy for GPIOB
    };
    uint8_t rx_buf[4] = {0};

    cs_assert();
    spi_write_read_blocking(spi_, tx_buf, rx_buf, 4);
    cs_release();

    return rx_buf[2] | (rx_buf[3] << 8);
}

void IoExpander::init_expander(uint8_t hw_addr, uint8_t iodira, uint8_t iodirb,
                                uint8_t gpintena, uint8_t gpintenb,
                                uint8_t defvala, uint8_t defvalb) {
    // Configure I/O direction
    write_register(hw_addr, MCP23S17::REG_IODIRA, iodira);
    write_register(hw_addr, MCP23S17::REG_IODIRB, iodirb);

    // Enable pull-ups on inputs (for fault lines)
    write_register(hw_addr, MCP23S17::REG_GPPUA, iodira);  // Pull-up where input
    write_register(hw_addr, MCP23S17::REG_GPPUB, iodirb);  // Pull-up where input

    // Configure interrupts if any enabled
    if (gpintena || gpintenb) {
        // Set default values for comparison (faults are active-low, expect high)
        write_register(hw_addr, MCP23S17::REG_DEFVALA, defvala);
        write_register(hw_addr, MCP23S17::REG_DEFVALB, defvalb);

        // Compare against DEFVAL (not previous value)
        write_register(hw_addr, MCP23S17::REG_INTCONA, gpintena);
        write_register(hw_addr, MCP23S17::REG_INTCONB, gpintenb);

        // Enable interrupts
        write_register(hw_addr, MCP23S17::REG_GPINTENA, gpintena);
        write_register(hw_addr, MCP23S17::REG_GPINTENB, gpintenb);
    }

    // Initialize outputs to safe defaults (all zeros - D_EN disabled, LDAC/CLR idle)
    write_register(hw_addr, MCP23S17::REG_GPIOA, 0x00);
    write_register(hw_addr, MCP23S17::REG_GPIOB, 0x00);

    // Clear cache
    if (hw_addr < EXPANDER_ADDR::NUM_EXPANDERS) {
        expander_cache_[hw_addr] = 0;
    }
}

void IoExpander::init(spi_inst_t* spi) {
    spi_ = spi;

    // Step 1: Enable HAEN on all expanders
    // Before HAEN is set, all expanders respond to address 0
    // Write IOCON with HAEN=1 and MIRROR=1 (to combine interrupt outputs)
    uint8_t iocon_value = MCP23S17::IOCON_HAEN | MCP23S17::IOCON_MIRROR;

    // Write to address 0 - all expanders will receive this
    write_register(0, MCP23S17::REG_IOCON, iocon_value);

    // Small delay to ensure all expanders have processed the command
    sleep_us(10);

    // Step 2: Now configure each expander individually
    // Expander 0: CS bits, D_EN, LDAC, CLR (all outputs on Port A, assumption)
    // TODO: Adjust based on actual schematic pin assignments
    init_expander(EXPANDER_ADDR::EXPANDER_0,
                  0x00,  // IODIRA: all outputs (CS0-4, D_EN, LDAC, CLR)
                  0xFF,  // IODIRB: all inputs (or unused - TODO)
                  0x00,  // No interrupts on Port A
                  0x00,  // No interrupts on Port B (TODO: may need faults here)
                  0xFF,  // DEFVALA (not used for outputs)
                  0xFF); // DEFVALB (expect high, fault = low)

    // Expander 1: Fault inputs from DACs 0-15 (assumption)
    // TODO: Adjust based on actual schematic pin assignments
    init_expander(EXPANDER_ADDR::EXPANDER_1,
                  0xFF,  // IODIRA: all inputs (faults 0-7)
                  0xFF,  // IODIRB: all inputs (faults 8-15)
                  0xFF,  // Enable interrupts on all Port A pins
                  0xFF,  // Enable interrupts on all Port B pins
                  0xFF,  // DEFVALA: expect high (no fault)
                  0xFF); // DEFVALB: expect high (no fault)

    // Expander 2: Fault inputs from DACs 16-23 + spare (assumption)
    // TODO: Adjust based on actual schematic pin assignments
    init_expander(EXPANDER_ADDR::EXPANDER_2,
                  0xFF,  // IODIRA: all inputs (faults 16-23)
                  0xFF,  // IODIRB: all inputs (spare/unused)
                  0xFF,  // Enable interrupts on all Port A pins
                  0x00,  // No interrupts on Port B (unused)
                  0xFF,  // DEFVALA: expect high (no fault)
                  0xFF); // DEFVALB: expect high (unused)

    // Clear any pending interrupts by reading GPIO
    clear_interrupts();
}

void IoExpander::set_dac_select(uint8_t board_id, uint8_t device_id) {
    // Calculate 5-bit CS address
    // Mapping: board_id (0-7) * 3 + device_id (0-2) = DAC index 0-23
    // CS[4:0] encodes this 5-bit address for the decoder tree
    uint8_t dac_index = (board_id * 3) + device_id;
    uint8_t cs_bits = dac_index & 0x1F;

    // Build Port A value with CS bits and D_EN
    // TODO: Update bit positions based on actual schematic
    uint8_t port_a_value = cs_bits;                         // CS0-CS4 in bits 0-4
    port_a_value |= (1 << SIGNAL_MAP::D_EN_BIT);           // Enable decoder

    // LDAC and CLR should be idle (high or low depending on polarity - TODO verify)
    // Assuming active-low for LDAC/CLR, so set them high (inactive)
    port_a_value |= (1 << SIGNAL_MAP::LDAC_BIT);
    port_a_value |= (1 << SIGNAL_MAP::CLR_BIT);

    // Write to the CS expander
    write_register(SIGNAL_MAP::CS_EXPANDER, MCP23S17::REG_GPIOA, port_a_value);

    // Update cache
    expander_cache_[SIGNAL_MAP::CS_EXPANDER] =
        (expander_cache_[SIGNAL_MAP::CS_EXPANDER] & 0xFF00) | port_a_value;
}

void IoExpander::deselect_dac() {
    // Clear D_EN to disable decoder tree, keep LDAC/CLR idle
    uint8_t port_a_value = 0;
    port_a_value |= (1 << SIGNAL_MAP::LDAC_BIT);  // LDAC idle (high)
    port_a_value |= (1 << SIGNAL_MAP::CLR_BIT);   // CLR idle (high)
    // D_EN = 0 (disabled), CS bits = 0 (don't care when disabled)

    write_register(SIGNAL_MAP::CS_EXPANDER, MCP23S17::REG_GPIOA, port_a_value);

    // Update cache
    expander_cache_[SIGNAL_MAP::CS_EXPANDER] =
        (expander_cache_[SIGNAL_MAP::CS_EXPANDER] & 0xFF00) | port_a_value;
}

void IoExpander::pulse_ldac() {
    // Read current value, pulse LDAC low, then restore
    uint8_t current = expander_cache_[SIGNAL_MAP::CS_EXPANDER] & 0xFF;

    // Clear LDAC bit (active-low pulse)
    uint8_t ldac_low = current & ~(1 << SIGNAL_MAP::LDAC_BIT);
    write_register(SIGNAL_MAP::CS_EXPANDER, MCP23S17::REG_GPIOA, ldac_low);

    // Brief delay for LDAC pulse (min ~20ns per datasheet, but add margin)
    sleep_us(1);

    // Restore LDAC high
    write_register(SIGNAL_MAP::CS_EXPANDER, MCP23S17::REG_GPIOA, current);
}

void IoExpander::assert_clear() {
    // Clear the CLR bit (active-low)
    uint8_t current = expander_cache_[SIGNAL_MAP::CS_EXPANDER] & 0xFF;
    uint8_t clr_low = current & ~(1 << SIGNAL_MAP::CLR_BIT);
    write_register(SIGNAL_MAP::CS_EXPANDER, MCP23S17::REG_GPIOA, clr_low);

    // Update cache
    expander_cache_[SIGNAL_MAP::CS_EXPANDER] =
        (expander_cache_[SIGNAL_MAP::CS_EXPANDER] & 0xFF00) | clr_low;
}

void IoExpander::release_clear() {
    // Set the CLR bit high (inactive)
    uint8_t current = expander_cache_[SIGNAL_MAP::CS_EXPANDER] & 0xFF;
    uint8_t clr_high = current | (1 << SIGNAL_MAP::CLR_BIT);
    write_register(SIGNAL_MAP::CS_EXPANDER, MCP23S17::REG_GPIOA, clr_high);

    // Update cache
    expander_cache_[SIGNAL_MAP::CS_EXPANDER] =
        (expander_cache_[SIGNAL_MAP::CS_EXPANDER] & 0xFF00) | clr_high;
}

uint32_t IoExpander::read_faults() {
    // Read fault inputs from expanders 1 and 2
    // Faults are active-low, so invert to get "1 = fault present"

    // Expander 1: faults 0-15 (Port A = 0-7, Port B = 8-15)
    uint16_t exp1 = read_gpio16(EXPANDER_ADDR::EXPANDER_1);

    // Expander 2: faults 16-23 (Port A only, Port B unused)
    uint8_t exp2_a = read_register(EXPANDER_ADDR::EXPANDER_2, MCP23S17::REG_GPIOA);

    // Combine and invert (active-low to active-high)
    uint32_t raw_faults = exp1 | (static_cast<uint32_t>(exp2_a) << 16);
    uint32_t faults = ~raw_faults & 0x00FFFFFF;  // Mask to 24 bits

    return faults;
}

void IoExpander::clear_interrupts() {
    // Reading GPIO or INTCAP clears interrupt flags
    // Read all expanders to clear any pending interrupts
    read_gpio16(EXPANDER_ADDR::EXPANDER_0);
    read_gpio16(EXPANDER_ADDR::EXPANDER_1);
    read_gpio16(EXPANDER_ADDR::EXPANDER_2);
}
