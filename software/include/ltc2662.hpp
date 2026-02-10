#ifndef LTC2662_HPP
#define LTC2662_HPP

#include "dac_device.hpp"

// LTC2662 Span Codes (Current DAC)
namespace LTC2662_SPAN {
    constexpr uint8_t HI_Z       = 0x0;  // Hi-Z (output disabled)
    constexpr uint8_t MA_3_125   = 0x1;  // 3.125 mA full scale
    constexpr uint8_t MA_6_25    = 0x2;  // 6.25 mA full scale
    constexpr uint8_t MA_12_5    = 0x3;  // 12.5 mA full scale
    constexpr uint8_t MA_25      = 0x4;  // 25 mA full scale
    constexpr uint8_t MA_50      = 0x5;  // 50 mA full scale
    constexpr uint8_t MA_100     = 0x6;  // 100 mA full scale
    constexpr uint8_t MA_200     = 0x7;  // 200 mA full scale
    constexpr uint8_t SWITCH_NEG = 0x8;  // Switch to V- (pull to negative supply)
    constexpr uint8_t MA_300     = 0xF;  // 300 mA full scale
}

// Full-scale current values in mA for each span code
constexpr float LTC2662_FS_CURRENT[] = {
    0.0,      // 0x0: Hi-Z
    3.125,    // 0x1
    6.25,     // 0x2
    12.5,     // 0x3
    25.0,     // 0x4
    50.0,     // 0x5
    100.0,    // 0x6
    200.0,    // 0x7
    0.0,      // 0x8: Switch to V-
    0.0, 0.0, 0.0, 0.0, 0.0, 0.0,  // 0x9-0xE: undefined (Hi-Z)
    300.0     // 0xF
};

// LTC2662: 5-channel current-source DAC (up to 300 mA)
// Available in 12-bit (LTC2662-12) and 16-bit (LTC2662-16) variants
class LTC2662 : public DacDevice {
public:
    static constexpr uint8_t NUM_CHANNELS = 5;

    // Default constructor (for array allocation, must call setup() before use)
    LTC2662() = default;

    // Constructor: board_id 0-7, device_id 0-1, resolution 12 or 16 bits
    LTC2662(SpiManager* spi, uint8_t board_id, uint8_t device_id, uint8_t resolution_bits = 16);

    // Setup method (alternative to constructor for pre-allocated objects)
    void setup(SpiManager* spi, uint8_t board_id, uint8_t device_id, uint8_t resolution_bits = 16);

    // DacDevice interface
    void init() override;
    void write_code(uint8_t channel, uint16_t code) override;
    void write_and_update(uint8_t channel, uint16_t code) override;
    void update_channel(uint8_t channel) override;
    void update_all() override;
    void set_span(uint8_t channel, uint8_t span_code) override;
    void power_down(uint8_t channel) override;
    void power_down_chip() override;
    uint8_t get_num_channels() const override { return NUM_CHANNELS; }
    const char* get_type_name() const override { return "LTC2662"; }
    uint8_t get_resolution() const override { return resolution_bits_; }
    uint16_t get_max_code() const override { return max_code_; }

    // LTC2662-specific methods

    // Set current in mA for a channel (uses current span setting)
    void set_current_ma(uint8_t channel, float current_ma);

    // Set span for all channels at once
    void set_span_all(uint8_t span_code);

    // Get full-scale current for current span setting
    float get_full_scale_ma(uint8_t channel) const;

    // Convert voltage to 16-bit code for given span
    uint16_t current_ma_to_code(uint8_t channel, float current_ma) const;

    // Configure device options
    // ref_disable: true = use external reference
    // thermal_disable: true = disable thermal shutdown
    // power_limit_disable: true = disable power limit protection
    // open_circuit_disable: true = disable open-circuit detection
    void configure(bool ref_disable, bool thermal_disable,
                   bool power_limit_disable, bool open_circuit_disable);

    // Read fault register via SPI readback (sends 24-bit NOP, returns first MISO byte)
    // FR[0-4]: Open-circuit on OUT[0-4]
    // FR5: Overtemperature (>175C)
    // FR6: Power limit (VDDx-VOUTx>10V at >=200mA, auto-reduced to 100mA)
    // FR7: Invalid SPI sequence length
    uint8_t read_fault_register();

    // Echo readback test (sends 32-bit NOP, captures fault byte + 24-bit echo)
    void echo_readback(uint8_t& fault_reg, uint32_t& echo);

private:
    uint8_t span_[NUM_CHANNELS] = {0};  // Current span setting per channel
    uint8_t resolution_bits_ = 16;       // 12 or 16 bit resolution
    uint16_t max_code_ = 65535;          // 4095 for 12-bit, 65535 for 16-bit
};

#endif // LTC2662_HPP
