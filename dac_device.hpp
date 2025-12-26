#ifndef DAC_DEVICE_HPP
#define DAC_DEVICE_HPP

#include <cstdint>
#include <memory>

// Forward declaration
class SpiManager;

// Common DAC command codes (shared between LTC2662 and LTC2664)
namespace DAC_CMD {
    constexpr uint8_t WRITE_CODE_N      = 0x0;  // Write code to channel n
    constexpr uint8_t UPDATE_N          = 0x1;  // Update channel n (power up)
    constexpr uint8_t WRITE_UPDATE_ALL  = 0x2;  // Write code to n, update all
    constexpr uint8_t WRITE_UPDATE_N    = 0x3;  // Write code to n, update n
    constexpr uint8_t POWER_DOWN_N      = 0x4;  // Power down channel n
    constexpr uint8_t POWER_DOWN_CHIP   = 0x5;  // Power down entire chip
    constexpr uint8_t WRITE_SPAN_N      = 0x6;  // Write span to channel n
    constexpr uint8_t CONFIG            = 0x7;  // Configuration command
    constexpr uint8_t WRITE_CODE_ALL    = 0x8;  // Write code to all channels
    constexpr uint8_t UPDATE_ALL        = 0x9;  // Update all channels
    constexpr uint8_t WRITE_UPDATE_ALL2 = 0xA;  // Write code to all, update all
    constexpr uint8_t MUX               = 0xB;  // Monitor MUX select
    constexpr uint8_t TOGGLE_SELECT     = 0xC;  // Toggle select
    constexpr uint8_t GLOBAL_TOGGLE     = 0xD;  // Global toggle
    constexpr uint8_t WRITE_SPAN_ALL    = 0xE;  // Write span to all channels
    constexpr uint8_t NOP               = 0xF;  // No operation
}

// Abstract DAC interface: one instance per physical DAC chip
class DacDevice {
public:
    virtual ~DacDevice() = default;

    // Initialize the DAC with default configuration
    virtual void init() = 0;

    // Write a 16-bit code to a channel
    virtual void write_code(uint8_t channel, uint16_t code) = 0;

    // Write code and immediately update the channel output
    virtual void write_and_update(uint8_t channel, uint16_t code) = 0;

    // Update channel from input register to DAC register
    virtual void update_channel(uint8_t channel) = 0;

    // Update all channels
    virtual void update_all() = 0;

    // Set the output span/range for a channel
    virtual void set_span(uint8_t channel, uint8_t span_code) = 0;

    // Power down a channel
    virtual void power_down(uint8_t channel) = 0;

    // Power down the entire chip
    virtual void power_down_chip() = 0;

    // Get the number of channels on this DAC
    virtual uint8_t get_num_channels() const = 0;

    // Get the DAC type name
    virtual const char* get_type_name() const = 0;

protected:
    // Low-level 24-bit SPI command
    void send_command(uint8_t command, uint8_t address, uint16_t data);

    SpiManager* spi_ = nullptr;
    uint8_t board_id_ = 0;
    uint8_t device_id_ = 0;
};

#endif // DAC_DEVICE_HPP
