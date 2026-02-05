#include "ltc2662.hpp"
#include "spi_manager.hpp"

LTC2662::LTC2662(SpiManager* spi, uint8_t board_id, uint8_t device_id, uint8_t resolution_bits) {
    setup(spi, board_id, device_id, resolution_bits);
}

void LTC2662::setup(SpiManager* spi, uint8_t board_id, uint8_t device_id, uint8_t resolution_bits) {
    spi_ = spi;
    board_id_ = board_id;
    device_id_ = device_id;
    resolution_bits_ = (resolution_bits == 12) ? 12 : 16;  // Only 12 or 16 valid
    max_code_ = (resolution_bits_ == 12) ? 4095 : 65535;
}

void LTC2662::init() {
    // Power-on state: all outputs Hi-Z, all registers cleared
    // Set a default span for all channels (3.125 mA is the lowest active span)
    set_span_all(LTC2662_SPAN::MA_3_125);

    // Update all channels to apply the span setting
    update_all();
}

void LTC2662::write_code(uint8_t channel, uint16_t code) {
    if (channel >= NUM_CHANNELS) return;
    code = (resolution_bits_ == 12) ? code << 4 : code;
    send_command(DAC_CMD::WRITE_CODE_N, channel, code);
}

void LTC2662::write_and_update(uint8_t channel, uint16_t code) {
    if (channel >= NUM_CHANNELS) return;
    code = (resolution_bits_ == 12) ? code << 4 : code;
    send_command(DAC_CMD::WRITE_UPDATE_N, channel, code);
}

void LTC2662::update_channel(uint8_t channel) {
    if (channel >= NUM_CHANNELS) return;
    send_command(DAC_CMD::UPDATE_N, channel, 0);
}

void LTC2662::update_all() {
    send_command(DAC_CMD::UPDATE_ALL, 0, 0);
}

void LTC2662::set_span(uint8_t channel, uint8_t span_code) {
    if (channel >= NUM_CHANNELS) return;

    // Span code goes in lower 4 bits of data
    send_command(DAC_CMD::WRITE_SPAN_N, channel, span_code & 0x0F);
    span_[channel] = span_code;
}

void LTC2662::set_span_all(uint8_t span_code) {
    // Write span to all channels
    send_command(DAC_CMD::WRITE_SPAN_ALL, 0, span_code & 0x0F);
    for (uint8_t i = 0; i < NUM_CHANNELS; i++) {
        span_[i] = span_code;
    }
}

void LTC2662::power_down(uint8_t channel) {
    if (channel >= NUM_CHANNELS) return;
    send_command(DAC_CMD::POWER_DOWN_N, channel, 0);
}

void LTC2662::power_down_chip() {
    send_command(DAC_CMD::POWER_DOWN_CHIP, 0, 0);
}

float LTC2662::get_full_scale_ma(uint8_t channel) const {
    if (channel >= NUM_CHANNELS) return 0.0f;
    uint8_t span = span_[channel];
    if (span > 0x0F) return 0.0f;
    return LTC2662_FS_CURRENT[span];
}

uint16_t LTC2662::current_ma_to_code(uint8_t channel, float current_ma) const {
    if (channel >= NUM_CHANNELS) return 0;
    
    float fs = get_full_scale_ma(channel);
    if (fs <= 0.0f) return 0;  // Hi-Z or invalid span
    
    // Clamp to valid range
    if (current_ma < 0.0f) current_ma = 0.0f;
    if (current_ma > fs) current_ma = fs;
    
    // Convert to code: CODE = (I_OUT / I_FS) * max_code
    uint16_t code = static_cast<uint16_t>((current_ma / fs) * static_cast<float>(max_code_) + 0.5f);
}

void LTC2662::set_current_ma(uint8_t channel, float current_ma) {
    if (channel >= NUM_CHANNELS) return;

    uint16_t code = current_ma_to_code(channel, current_ma);
    write_and_update(channel, code);
}

void LTC2662::configure(bool ref_disable, bool thermal_disable,
                        bool power_limit_disable, bool open_circuit_disable) {
    // Config bits: [D3:OC | D2:PL | D1:TS | D0:RD]
    uint16_t config = 0;
    if (ref_disable)          config |= 0x01;  // D0: Reference Disable
    if (thermal_disable)      config |= 0x02;  // D1: Thermal Shutdown Disable
    if (power_limit_disable)  config |= 0x04;  // D2: Power Limit Protection Disable
    if (open_circuit_disable) config |= 0x08;  // D3: Open-Circuit Detection Disable

    send_command(DAC_CMD::CONFIG, 0, config);
}
