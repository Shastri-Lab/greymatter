#ifndef LTC2664_HPP
#define LTC2664_HPP

#include "dac_device.hpp"

// LTC2664 Span Codes (Voltage DAC)
namespace LTC2664_SPAN {
    constexpr uint8_t V_0_5      = 0x0;  // 0V to 5V (unipolar)
    constexpr uint8_t V_0_10     = 0x1;  // 0V to 10V (unipolar)
    constexpr uint8_t V_PM5      = 0x2;  // ±5V (bipolar)
    constexpr uint8_t V_PM10     = 0x3;  // ±10V (bipolar)
    constexpr uint8_t V_PM2_5    = 0x4;  // ±2.5V (bipolar)
}

// Span configuration structure
struct LTC2664SpanInfo {
    float min_v;      // Minimum voltage
    float max_v;      // Maximum voltage
    bool bipolar;     // True if bipolar range
};

// Span info lookup table
constexpr LTC2664SpanInfo LTC2664_SPAN_INFO[] = {
    {0.0f,   5.0f,  false},  // 0x0: 0V to 5V
    {0.0f,  10.0f,  false},  // 0x1: 0V to 10V
    {-5.0f,  5.0f,  true},   // 0x2: ±5V
    {-10.0f, 10.0f, true},   // 0x3: ±10V
    {-2.5f,  2.5f,  true},   // 0x4: ±2.5V
};

// LTC2664: 4-channel, 16-bit voltage-output DAC (±10V)
class LTC2664 : public DacDevice {
public:
    static constexpr uint8_t NUM_CHANNELS = 4;

    // Default constructor (for array allocation, must call setup() before use)
    LTC2664() = default;

    // Constructor: board_id 0-7, device_id 2 (LTC2664 is third DAC on each board)
    LTC2664(SpiManager* spi, uint8_t board_id, uint8_t device_id);

    // Setup method (alternative to constructor for pre-allocated objects)
    void setup(SpiManager* spi, uint8_t board_id, uint8_t device_id);

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
    const char* get_type_name() const override { return "LTC2664"; }

    // LTC2664-specific methods

    // Set voltage in volts for a channel (uses current span setting)
    void set_voltage(uint8_t channel, float voltage);

    // Set span for all channels at once
    void set_span_all(uint8_t span_code);

    // Get voltage range for current span setting
    float get_min_voltage(uint8_t channel) const;
    float get_max_voltage(uint8_t channel) const;
    bool is_bipolar(uint8_t channel) const;

    // Configure device options
    // ref_disable: true = use external reference
    // thermal_disable: true = disable thermal shutdown
    void configure(bool ref_disable, bool thermal_disable);

    // Convert voltage to 16-bit code for given span
    uint16_t voltage_to_code(uint8_t channel, float voltage) const;

    // Convert 16-bit code to voltage for given span
    float code_to_voltage(uint8_t channel, uint16_t code) const;

private:
    uint8_t span_[NUM_CHANNELS] = {0};  // Current span setting per channel
};

#endif // LTC2664_HPP
