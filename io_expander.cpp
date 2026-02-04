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
    // Expander 0: Control signals
    //   Port A: CS0-CS4 (pins 4-0), D_EN (pin 5) - all outputs
    //   Port B: LDAC (pin 0), CLR (pin 7) - outputs; other pins unused
    init_expander(EXPANDER_ADDR::EXPANDER_0,
                  0x00,  // IODIRA: all outputs (CS4-CS0 on pins 0-4, D_EN on pin 5)
                  0x00,  // IODIRB: all outputs (LDAC on pin 0, CLR on pin 7)
                  0x00,  // No interrupts on Port A
                  0x00,  // No interrupts on Port B
                  0x00,  // DEFVALA (not used for outputs)
                  0x00); // DEFVALB (not used for outputs)

    // Set initial state: D_EN=0 (disabled), LDAC=1 (idle), CLR=1 (idle)
    write_register(EXPANDER_ADDR::EXPANDER_0, MCP23S17::REG_GPIOA, 0x00);
    write_register(EXPANDER_ADDR::EXPANDER_0, MCP23S17::REG_GPIOB,
                   (1 << SIGNAL_MAP::LDAC_BIT) | (1 << SIGNAL_MAP::CLR_BIT));
    expander_cache_[EXPANDER_ADDR::EXPANDER_0] =
        ((1 << SIGNAL_MAP::LDAC_BIT) | (1 << SIGNAL_MAP::CLR_BIT)) << 8;

    // Expander 1: LTC2662 fault inputs (current DACs)
    //   Port A: Boards 0-3, DACs 0-1 (pins 0-7 = FAULT_11, FAULT_12, FAULT_21, ...)
    //   Port B: Boards 4-7, DACs 0-1 (pins 0-7 = FAULT_51, FAULT_52, FAULT_61, ...)
    init_expander(EXPANDER_ADDR::EXPANDER_1,
                  0xFF,  // IODIRA: all inputs
                  0xFF,  // IODIRB: all inputs
                  0xFF,  // Enable interrupts on all Port A pins
                  0xFF,  // Enable interrupts on all Port B pins
                  0xFF,  // DEFVALA: expect high (no fault)
                  0xFF); // DEFVALB: expect high (no fault)

    // Expander 2: LTC2664 temperature fault inputs (voltage DACs)
    //   Port A: All 8 boards, DAC 2 (pins 0-7 = TEMP_1 through TEMP_8)
    //   Port B: unused
    init_expander(EXPANDER_ADDR::EXPANDER_2,
                  0xFF,  // IODIRA: all inputs (temperature faults)
                  0xFF,  // IODIRB: all inputs (unused, but configure as inputs)
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

    // Build Port A value with CS bits (bit-reversed) and D_EN
    // Hardware wiring: CS4→pin0, CS3→pin1, CS2→pin2, CS1→pin3, CS0→pin4
    // So we need to reverse the bit order of the 5-bit address
    uint8_t port_a_value = 0;
    port_a_value |= ((dac_index >> 0) & 1) << SIGNAL_MAP::CS0_BIT;  // CS0 (bit 0 of index) → pin 4
    port_a_value |= ((dac_index >> 1) & 1) << SIGNAL_MAP::CS1_BIT;  // CS1 (bit 1 of index) → pin 3
    port_a_value |= ((dac_index >> 2) & 1) << SIGNAL_MAP::CS2_BIT;  // CS2 (bit 2 of index) → pin 2
    port_a_value |= ((dac_index >> 3) & 1) << SIGNAL_MAP::CS3_BIT;  // CS3 (bit 3 of index) → pin 1
    port_a_value |= ((dac_index >> 4) & 1) << SIGNAL_MAP::CS4_BIT;  // CS4 (bit 4 of index) → pin 0
    port_a_value |= (1 << SIGNAL_MAP::D_EN_BIT);                    // Enable decoder tree

    // Write to the control expander Port A
    write_register(SIGNAL_MAP::CTRL_EXPANDER, MCP23S17::REG_GPIOA, port_a_value);

    // Update cache (Port A in low byte)
    expander_cache_[SIGNAL_MAP::CTRL_EXPANDER] =
        (expander_cache_[SIGNAL_MAP::CTRL_EXPANDER] & 0xFF00) | port_a_value;
}

void IoExpander::deselect_dac() {
    // Clear D_EN to disable decoder tree (Port A)
    // D_EN = 0 (disabled), CS bits = 0 (don't care when disabled)
    uint8_t port_a_value = 0;

    write_register(SIGNAL_MAP::CTRL_EXPANDER, MCP23S17::REG_GPIOA, port_a_value);

    // Update cache (Port A in low byte, Port B unchanged)
    expander_cache_[SIGNAL_MAP::CTRL_EXPANDER] =
        (expander_cache_[SIGNAL_MAP::CTRL_EXPANDER] & 0xFF00) | port_a_value;
}

void IoExpander::pulse_ldac() {
    // LDAC is on Port B, bit 0 (active-low)
    // Read current Port B value from cache (high byte)
    uint8_t current_b = (expander_cache_[SIGNAL_MAP::CTRL_EXPANDER] >> 8) & 0xFF;

    // Clear LDAC bit (active-low pulse)
    uint8_t ldac_low = current_b & ~(1 << SIGNAL_MAP::LDAC_BIT);
    write_register(SIGNAL_MAP::CTRL_EXPANDER, MCP23S17::REG_GPIOB, ldac_low);

    // Brief delay for LDAC pulse (min ~20ns per datasheet, but add margin)
    sleep_us(1);

    // Restore LDAC high
    write_register(SIGNAL_MAP::CTRL_EXPANDER, MCP23S17::REG_GPIOB, current_b);
}

void IoExpander::assert_clear() {
    // CLR is on Port B, bit 7 (active-low)
    uint8_t current_b = (expander_cache_[SIGNAL_MAP::CTRL_EXPANDER] >> 8) & 0xFF;
    uint8_t clr_low = current_b & ~(1 << SIGNAL_MAP::CLR_BIT);
    write_register(SIGNAL_MAP::CTRL_EXPANDER, MCP23S17::REG_GPIOB, clr_low);

    // Update cache (Port B in high byte)
    expander_cache_[SIGNAL_MAP::CTRL_EXPANDER] =
        (expander_cache_[SIGNAL_MAP::CTRL_EXPANDER] & 0x00FF) | (static_cast<uint16_t>(clr_low) << 8);
}

void IoExpander::release_clear() {
    // CLR is on Port B, bit 7 (set high = inactive)
    uint8_t current_b = (expander_cache_[SIGNAL_MAP::CTRL_EXPANDER] >> 8) & 0xFF;
    uint8_t clr_high = current_b | (1 << SIGNAL_MAP::CLR_BIT);
    write_register(SIGNAL_MAP::CTRL_EXPANDER, MCP23S17::REG_GPIOB, clr_high);

    // Update cache (Port B in high byte)
    expander_cache_[SIGNAL_MAP::CTRL_EXPANDER] =
        (expander_cache_[SIGNAL_MAP::CTRL_EXPANDER] & 0x00FF) | (static_cast<uint16_t>(clr_high) << 8);
}

uint32_t IoExpander::read_faults() {
    // Read fault inputs from expanders 1 and 2
    // Faults are active-low, so invert to get "1 = fault present"
    //
    // Hardware layout:
    //   Expander 1 Port A: FAULT_11(B0D0), FAULT_12(B0D1), FAULT_21(B1D0), FAULT_22(B1D1),
    //                      FAULT_31(B2D0), FAULT_32(B2D1), FAULT_41(B3D0), FAULT_42(B3D1)
    //   Expander 1 Port B: FAULT_51(B4D0), FAULT_52(B4D1), FAULT_61(B5D0), FAULT_62(B5D1),
    //                      FAULT_71(B6D0), FAULT_72(B6D1), FAULT_81(B7D0), FAULT_82(B7D1)
    //   Expander 2 Port A: TEMP_1(B0D2), TEMP_2(B1D2), ..., TEMP_8(B7D2)
    //
    // Output format: 24-bit mask where bit N = fault on DAC index N
    //   DAC index = board_id * 3 + device_id
    //   So bit 0 = B0D0, bit 1 = B0D1, bit 2 = B0D2, bit 3 = B1D0, etc.

    uint16_t exp1 = read_gpio16(SIGNAL_MAP::FAULT_EXPANDER);
    uint8_t exp2_a = read_register(SIGNAL_MAP::TEMP_EXPANDER, MCP23S17::REG_GPIOA);

    // Invert (active-low to active-high)
    uint16_t ltc2662_faults = ~exp1;      // 16 current DAC faults
    uint8_t ltc2664_faults = ~exp2_a;     // 8 voltage DAC temperature faults

    // Reorganize bits to match DAC index scheme
    // For each board, we need: DAC0 fault, DAC1 fault, DAC2 temp fault
    uint32_t faults = 0;

    for (int board = 0; board < 8; board++) {
        int dac_base_index = board * 3;

        // LTC2662 faults are at pins (board*2) and (board*2+1) within exp1
        // Boards 0-3 are in Port A (low byte), Boards 4-7 are in Port B (high byte)
        int fault_pin_base = board * 2;
        uint8_t dac0_fault = (ltc2662_faults >> fault_pin_base) & 1;
        uint8_t dac1_fault = (ltc2662_faults >> (fault_pin_base + 1)) & 1;

        // LTC2664 temperature fault is at pin (board) within exp2_a
        uint8_t dac2_fault = (ltc2664_faults >> board) & 1;

        // Set output bits
        faults |= (static_cast<uint32_t>(dac0_fault) << dac_base_index);
        faults |= (static_cast<uint32_t>(dac1_fault) << (dac_base_index + 1));
        faults |= (static_cast<uint32_t>(dac2_fault) << (dac_base_index + 2));
    }

    return faults;
}

void IoExpander::clear_interrupts() {
    // Reading GPIO or INTCAP clears interrupt flags
    // Read all expanders to clear any pending interrupts
    read_gpio16(EXPANDER_ADDR::EXPANDER_0);
    read_gpio16(EXPANDER_ADDR::EXPANDER_1);
    read_gpio16(EXPANDER_ADDR::EXPANDER_2);
}
