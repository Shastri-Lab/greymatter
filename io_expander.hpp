#ifndef IO_EXPANDER_HPP
#define IO_EXPANDER_HPP

#include <cstdint>
#include "hardware/spi.h"
#include "pico/stdlib.h"

// MCP23S17 Register Addresses (BANK=0 mode, paired registers)
namespace MCP23S17 {
    // I/O Direction Registers
    constexpr uint8_t REG_IODIRA   = 0x00;  // Port A direction (1=input, 0=output)
    constexpr uint8_t REG_IODIRB   = 0x01;  // Port B direction

    // Input Polarity Registers
    constexpr uint8_t REG_IPOLA    = 0x02;  // Port A polarity (1=inverted)
    constexpr uint8_t REG_IPOLB    = 0x03;  // Port B polarity

    // Interrupt-on-Change Enable Registers
    constexpr uint8_t REG_GPINTENA = 0x04;  // Port A interrupt enable
    constexpr uint8_t REG_GPINTENB = 0x05;  // Port B interrupt enable

    // Default Compare Value Registers
    constexpr uint8_t REG_DEFVALA  = 0x06;  // Port A default compare value
    constexpr uint8_t REG_DEFVALB  = 0x07;  // Port B default compare value

    // Interrupt Control Registers
    constexpr uint8_t REG_INTCONA  = 0x08;  // Port A interrupt control (1=compare to DEFVAL)
    constexpr uint8_t REG_INTCONB  = 0x09;  // Port B interrupt control

    // Configuration Register
    constexpr uint8_t REG_IOCON    = 0x0A;  // Shared configuration register

    // Pull-up Resistor Registers
    constexpr uint8_t REG_GPPUA    = 0x0C;  // Port A pull-up enable
    constexpr uint8_t REG_GPPUB    = 0x0D;  // Port B pull-up enable

    // Interrupt Flag Registers (read-only)
    constexpr uint8_t REG_INTFA    = 0x0E;  // Port A interrupt flags
    constexpr uint8_t REG_INTFB    = 0x0F;  // Port B interrupt flags

    // Interrupt Capture Registers (read-only)
    constexpr uint8_t REG_INTCAPA  = 0x10;  // Port A captured value at interrupt
    constexpr uint8_t REG_INTCAPB  = 0x11;  // Port B captured value at interrupt

    // GPIO Registers
    constexpr uint8_t REG_GPIOA    = 0x12;  // Port A GPIO
    constexpr uint8_t REG_GPIOB    = 0x13;  // Port B GPIO

    // Output Latch Registers
    constexpr uint8_t REG_OLATA    = 0x14;  // Port A output latch
    constexpr uint8_t REG_OLATB    = 0x15;  // Port B output latch

    // IOCON Register Bits
    constexpr uint8_t IOCON_BANK   = 0x80;  // Register addressing mode
    constexpr uint8_t IOCON_MIRROR = 0x40;  // Mirror INTA/INTB
    constexpr uint8_t IOCON_SEQOP  = 0x20;  // Sequential operation disable
    constexpr uint8_t IOCON_DISSLW = 0x10;  // Slew rate disable
    constexpr uint8_t IOCON_HAEN   = 0x08;  // Hardware address enable
    constexpr uint8_t IOCON_ODR    = 0x04;  // Open-drain output
    constexpr uint8_t IOCON_INTPOL = 0x02;  // Interrupt polarity (1=active-high)

    // Control byte construction
    constexpr uint8_t OPCODE_BASE  = 0x40;  // Fixed bits [7:4] = 0100

    inline constexpr uint8_t write_opcode(uint8_t hw_addr) {
        return OPCODE_BASE | ((hw_addr & 0x07) << 1);
    }

    inline constexpr uint8_t read_opcode(uint8_t hw_addr) {
        return OPCODE_BASE | ((hw_addr & 0x07) << 1) | 0x01;
    }
}

// Hardware GPIO pin assignments (from CLAUDE.md)
namespace HW_PINS {
    constexpr uint SPI_MISO = 16;
    constexpr uint SPI_CS   = 17;
    constexpr uint SPI_CLK  = 18;
    constexpr uint SPI_MOSI = 19;
    constexpr uint FAULT    = 20;
    constexpr uint LEVEL_SHIFT_OE = 21;
    constexpr uint EXPANDER_RESET = 22;
}

// MCP23S17 Hardware Addresses
namespace EXPANDER_ADDR {
    constexpr uint8_t EXPANDER_0 = 0;  // A2=0, A1=0, A0=0
    constexpr uint8_t EXPANDER_1 = 1;  // A2=0, A1=0, A0=1
    constexpr uint8_t EXPANDER_2 = 2;  // A2=0, A1=1, A0=0
    constexpr uint8_t NUM_EXPANDERS = 3;
}

// IO Expander Signal Mapping (TODO: populate from schematic)
// These are placeholders - update when pin assignments are known
namespace SIGNAL_MAP {
    // Expander 0: CS bits and control signals (assumption - verify with schematic)
    constexpr uint8_t CS_EXPANDER = 0;     // Which expander has CS0-CS4, D_EN
    constexpr uint8_t CS_PORT = 0;         // 0 = Port A, 1 = Port B

    // CS bit positions within the port (TODO: verify)
    constexpr uint8_t CS0_BIT = 0;
    constexpr uint8_t CS1_BIT = 1;
    constexpr uint8_t CS2_BIT = 2;
    constexpr uint8_t CS3_BIT = 3;
    constexpr uint8_t CS4_BIT = 4;
    constexpr uint8_t D_EN_BIT = 5;        // Decoder enable
    constexpr uint8_t LDAC_BIT = 6;        // Load DAC
    constexpr uint8_t CLR_BIT = 7;         // Clear DAC

    // Fault inputs (likely on expanders 1 and 2 - TODO: verify)
    // Faults are active-low from DACs
}

// Handles MCP23S17 IO expanders for chip select routing and fault monitoring
class IoExpander {
public:
    // Initialize all three MCP23S17 expanders
    // Must be called after SPI peripheral is initialized
    void init(spi_inst_t* spi);

    // Set the 5-bit DAC select address and enable decoder tree
    // board_id: 0-7, device_id: 0-2 (2x LTC2662 + 1x LTC2664 per board)
    void set_dac_select(uint8_t board_id, uint8_t device_id);

    // Disable the decoder tree (deselect all DACs)
    void deselect_dac();

    // Pulse LDAC low to update all DAC outputs
    void pulse_ldac();

    // Assert CLR to clear all DACs to zero
    void assert_clear();
    void release_clear();

    // Read fault status from all DACs
    // Returns 24-bit mask where bit N = fault on DAC N (active = fault present)
    uint32_t read_faults();

    // Clear any pending interrupt flags by reading INTCAP registers
    void clear_interrupts();

    // Low-level register access (for debugging/testing)
    void write_register(uint8_t hw_addr, uint8_t reg, uint8_t value);
    uint8_t read_register(uint8_t hw_addr, uint8_t reg);

    // Write 16-bit value to both ports (GPIOA + GPIOB)
    void write_gpio16(uint8_t hw_addr, uint16_t value);
    uint16_t read_gpio16(uint8_t hw_addr);

private:
    spi_inst_t* spi_ = nullptr;

    // Cached output latch values for read-modify-write operations
    uint16_t expander_cache_[EXPANDER_ADDR::NUM_EXPANDERS] = {0, 0, 0};

    // Assert/release CS for IO expander communication
    void cs_assert();
    void cs_release();

    // Configure single expander during init
    void init_expander(uint8_t hw_addr, uint8_t iodira, uint8_t iodirb,
                       uint8_t gpintena, uint8_t gpintenb,
                       uint8_t defvala, uint8_t defvalb);
};

#endif // IO_EXPANDER_HPP
