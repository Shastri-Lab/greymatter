#include "ltc2664.hpp"
#include "spi_manager.hpp"

LTC2664::LTC2664(SpiManager* spi, uint8_t board_id, uint8_t device_id, uint8_t resolution_bits) {
    setup(spi, board_id, device_id, resolution_bits);
}

void LTC2664::setup(SpiManager* spi, uint8_t board_id, uint8_t device_id, uint8_t resolution_bits) {
    spi_ = spi;
    board_id_ = board_id;
    device_id_ = device_id;
    resolution_bits_ = (resolution_bits == 12) ? 12 : 16;  // Only 12 or 16 valid
    max_code_ = (resolution_bits_ == 12) ? 4095 : 65535;
}

void LTC2664::init() {
    // Power-on state depends on MSPAN pins (hardware configuration)
    // Assuming SoftSpan mode (all MSPAN pins = VCC): 0V to 5V, zero-scale
    // Set a default span for all channels (±10V is most versatile)
    set_span_all(LTC2664_SPAN::V_PM10);

    // Update all channels to apply the span setting
    update_all();
}

void LTC2664::write_code(uint8_t channel, uint16_t code) {
    if (channel >= NUM_CHANNELS) return;
    send_command(DAC_CMD::WRITE_CODE_N, channel, code);
}

void LTC2664::write_and_update(uint8_t channel, uint16_t code) {
    if (channel >= NUM_CHANNELS) return;
    send_command(DAC_CMD::WRITE_UPDATE_N, channel, code);
}

void LTC2664::update_channel(uint8_t channel) {
    if (channel >= NUM_CHANNELS) return;
    send_command(DAC_CMD::UPDATE_N, channel, 0);
}

void LTC2664::update_all() {
    send_command(DAC_CMD::UPDATE_ALL, 0, 0);
}

void LTC2664::set_span(uint8_t channel, uint8_t span_code) {
    if (channel >= NUM_CHANNELS) return;
    if (span_code > LTC2664_SPAN::V_PM2_5) return;

    // Span code goes in lower 3 bits of data
    send_command(DAC_CMD::WRITE_SPAN_N, channel, span_code & 0x07);
    span_[channel] = span_code;
}

void LTC2664::set_span_all(uint8_t span_code) {
    if (span_code > LTC2664_SPAN::V_PM2_5) return;

    // Write span to all channels
    send_command(DAC_CMD::WRITE_SPAN_ALL, 0, span_code & 0x07);
    for (uint8_t i = 0; i < NUM_CHANNELS; i++) {
        span_[i] = span_code;
    }
}

void LTC2664::power_down(uint8_t channel) {
    if (channel >= NUM_CHANNELS) return;
    send_command(DAC_CMD::POWER_DOWN_N, channel, 0);
}

void LTC2664::power_down_chip() {
    send_command(DAC_CMD::POWER_DOWN_CHIP, 0, 0);
}

float LTC2664::get_min_voltage(uint8_t channel) const {
    if (channel >= NUM_CHANNELS) return 0.0f;
    uint8_t span = span_[channel];
    if (span > LTC2664_SPAN::V_PM2_5) return 0.0f;
    return LTC2664_SPAN_INFO[span].min_v;
}

float LTC2664::get_max_voltage(uint8_t channel) const {
    if (channel >= NUM_CHANNELS) return 0.0f;
    uint8_t span = span_[channel];
    if (span > LTC2664_SPAN::V_PM2_5) return 0.0f;
    return LTC2664_SPAN_INFO[span].max_v;
}

bool LTC2664::is_bipolar(uint8_t channel) const {
    if (channel >= NUM_CHANNELS) return false;
    uint8_t span = span_[channel];
    if (span > LTC2664_SPAN::V_PM2_5) return false;
    return LTC2664_SPAN_INFO[span].bipolar;
}

uint16_t LTC2664::voltage_to_code(uint8_t channel, float voltage) const {
    if (channel >= NUM_CHANNELS) return 0;

    float min_v = get_min_voltage(channel);
    float max_v = get_max_voltage(channel);
    float range = max_v - min_v;

    if (range <= 0.0f) return 0;

    // Clamp voltage to valid range
    if (voltage < min_v) voltage = min_v;
    if (voltage > max_v) voltage = max_v;

    // For unipolar: CODE = (V_OUT / V_FS) * max_code
    // For bipolar: CODE = ((V_OUT - V_MIN) / RANGE) * max_code
    float normalized = (voltage - min_v) / range;
    return static_cast<uint16_t>(normalized * static_cast<float>(max_code_) + 0.5f);
}

float LTC2664::code_to_voltage(uint8_t channel, uint16_t code) const {
    if (channel >= NUM_CHANNELS) return 0.0f;

    float min_v = get_min_voltage(channel);
    float max_v = get_max_voltage(channel);
    float range = max_v - min_v;

    if (range <= 0.0f) return 0.0f;

    float normalized = static_cast<float>(code) / static_cast<float>(max_code_);
    return min_v + (normalized * range);
}

void LTC2664::set_voltage(uint8_t channel, float voltage) {
    if (channel >= NUM_CHANNELS) return;

    uint16_t code = voltage_to_code(channel, voltage);
    write_and_update(channel, code);
}

void LTC2664::configure(bool ref_disable, bool thermal_disable) {
    // Config bits: [D1:TS | D0:RD]
    uint16_t config = 0;
    if (ref_disable)     config |= 0x01;  // D0: Reference Disable
    if (thermal_disable) config |= 0x02;  // D1: Thermal Shutdown Disable

    send_command(DAC_CMD::CONFIG, 0, config);
}
