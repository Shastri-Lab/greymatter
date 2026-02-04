#include "board_manager.hpp"
#include "utils.hpp"
#include "cal_storage.hpp"
#include <cstdio>
#include <cstring>

BoardManager::BoardManager(SpiManager& spi) : spi_(spi) {
    // Initialize DAC pointers and storage
    // Each board has: DAC0=LTC2662, DAC1=LTC2662, DAC2=LTC2664
    for (uint8_t board = 0; board < NUM_BOARDS; board++) {
        // LTC2662 current DACs (device_id 0 and 1)
        current_dacs_[board][0] = nullptr;
        current_dacs_[board][1] = nullptr;
        voltage_dacs_[board] = nullptr;

        // Set default resolutions
        resolution_[board][0] = DEFAULT_CURRENT_DAC_RESOLUTION;
        resolution_[board][1] = DEFAULT_CURRENT_DAC_RESOLUTION;
        resolution_[board][2] = DEFAULT_VOLTAGE_DAC_RESOLUTION;

        // Initialize serial numbers to empty
        serial_numbers_[board][0] = '\0';

        // Initialize calibration to defaults (gain=1, offset=0, disabled)
        for (uint8_t dac = 0; dac < DACS_PER_BOARD; dac++) {
            for (uint8_t ch = 0; ch < MAX_CHANNELS_PER_DAC; ch++) {
                calibration_[board][dac][ch].gain = 1.0f;
                calibration_[board][dac][ch].offset = 0.0f;
                calibration_[board][dac][ch].enabled = false;
            }
        }
    }
}

void BoardManager::init_all() {
    // Setup and initialize all DAC devices
    for (uint8_t board = 0; board < NUM_BOARDS; board++) {
        // Setup LTC2662 current DACs at device_id 0 and 1
        uint8_t idx0 = board * 2;
        uint8_t idx1 = board * 2 + 1;

        current_dac_storage_[idx0].setup(&spi_, board, 0, resolution_[board][0]);
        current_dac_storage_[idx1].setup(&spi_, board, 1, resolution_[board][1]);
        current_dacs_[board][0] = &current_dac_storage_[idx0];
        current_dacs_[board][1] = &current_dac_storage_[idx1];

        // Setup LTC2664 voltage DAC at device_id 2
        voltage_dac_storage_[board].setup(&spi_, board, 2, resolution_[board][2]);
        voltage_dacs_[board] = &voltage_dac_storage_[board];

        // Initialize each DAC
        current_dacs_[board][0]->init();
        current_dacs_[board][1]->init();
        voltage_dacs_[board]->init();
    }

    // Load calibration data from flash (if valid data exists)
    CalStorage::load_from_flash(*this);
}

void BoardManager::reset_all() {
    // Power down and re-initialize all DACs
    for (uint8_t board = 0; board < NUM_BOARDS; board++) {
        if (current_dacs_[board][0]) current_dacs_[board][0]->power_down_chip();
        if (current_dacs_[board][1]) current_dacs_[board][1]->power_down_chip();
        if (voltage_dacs_[board]) voltage_dacs_[board]->power_down_chip();
    }

    // Re-initialize
    init_all();
}

DacDevice* BoardManager::get_dac(uint8_t board, uint8_t dac) {
    if (board >= NUM_BOARDS || dac >= DACS_PER_BOARD) {
        return nullptr;
    }

    if (dac < 2) {
        return current_dacs_[board][dac];
    } else {
        return voltage_dacs_[board];
    }
}

uint8_t BoardManager::get_dac_type(uint8_t board, uint8_t dac) {
    // 0 = LTC2662 (current), 1 = LTC2664 (voltage)
    return (dac == 2) ? 1 : 0;
}

void BoardManager::set_resolution(uint8_t board, uint8_t dac, uint8_t resolution_bits) {
    if (board >= NUM_BOARDS || dac >= DACS_PER_BOARD) return;
    resolution_[board][dac] = (resolution_bits == 12) ? 12 : 16;
}

uint8_t BoardManager::get_resolution(uint8_t board, uint8_t dac) {
    if (board >= NUM_BOARDS || dac >= DACS_PER_BOARD) return 16;
    return resolution_[board][dac];
}

std::string BoardManager::execute_idn() {
    return "GreyMatter,DAC Controller,001,0.1"; // TODO: set some global variables to populate the version information
}

std::string BoardManager::execute_fault_query() {
#ifdef SINGLE_BOARD_MODE
    // Single-board mode: FAULT is NAND of all 3 DAC faults
    // Can only detect "any fault" vs "no fault", not which DAC
    if (spi_.is_fault_active()) {
        return "FAULT:ACTIVE";
    }
    return "OK";
#else
    // Multi-board mode: Per-DAC fault detection via IO expanders
    if (spi_.is_fault_active()) {
        uint32_t faults = spi_.io_expander().read_faults();
        char buf[32];
        snprintf(buf, sizeof(buf), "FAULT:0x%06lX", faults);
        return buf;
    }
    return "OK";
#endif
}

std::string BoardManager::execute_set_voltage(const ScpiCommand& cmd) {
    if (cmd.board_id < 0 || cmd.dac_id < 0 || cmd.channel_id < 0) {
        return "ERROR:Missing address";
    }

    // Voltage commands only apply to LTC2664 (DAC 2)
    if (cmd.dac_id != 2) {
        return "ERROR:Use CURR for current DACs";
    }

    LTC2664* dac = voltage_dacs_[cmd.board_id];
    if (!dac) {
        return "ERROR:DAC not initialized";
    }

    if (cmd.channel_id >= dac->get_num_channels()) {
        return "ERROR:Invalid channel";
    }

    // Apply calibration if enabled
    // Calibrated output = (ideal_output * gain) + offset
    float voltage = cmd.float_value;
    const ChannelCalibration* cal = get_calibration(cmd.board_id, cmd.dac_id, cmd.channel_id);
    if (cal && cal->enabled) {
        voltage = (voltage * cal->gain) + cal->offset;
    }

    dac->set_voltage(cmd.channel_id, voltage);
    return "OK";
}

std::string BoardManager::execute_set_current(const ScpiCommand& cmd) {
    if (cmd.board_id < 0 || cmd.dac_id < 0 || cmd.channel_id < 0) {
        return "ERROR:Missing address";
    }

    // Current commands only apply to LTC2662 (DAC 0, 1)
    if (cmd.dac_id == 2) {
        return "ERROR:Use VOLT for voltage DACs";
    }

    LTC2662* dac = current_dacs_[cmd.board_id][cmd.dac_id];
    if (!dac) {
        return "ERROR:DAC not initialized";
    }

    if (cmd.channel_id >= dac->get_num_channels()) {
        return "ERROR:Invalid channel";
    }

    // Apply calibration if enabled
    // Calibrated output = (ideal_output * gain) + offset
    float current_ma = cmd.float_value;
    const ChannelCalibration* cal = get_calibration(cmd.board_id, cmd.dac_id, cmd.channel_id);
    if (cal && cal->enabled) {
        current_ma = (current_ma * cal->gain) + cal->offset;
    }

    dac->set_current_ma(cmd.channel_id, current_ma);
    return "OK";
}

std::string BoardManager::execute_set_code(const ScpiCommand& cmd) {
    if (cmd.board_id < 0 || cmd.dac_id < 0 || cmd.channel_id < 0) {
        return "ERROR:Missing address";
    }

    DacDevice* dac = get_dac(cmd.board_id, cmd.dac_id);
    if (!dac) {
        return "ERROR:DAC not initialized";
    }

    if (cmd.channel_id >= dac->get_num_channels()) {
        return "ERROR:Invalid channel";
    }

    // Validate code is within DAC's resolution range
    if (cmd.int_value > dac->get_max_code()) {
        char buf[64];
        snprintf(buf, sizeof(buf), "ERROR:Code exceeds max (%u for %u-bit)",
                 dac->get_max_code(), dac->get_resolution());
        return buf;
    }

    dac->write_and_update(cmd.channel_id, cmd.int_value);
    return "OK";
}

std::string BoardManager::execute_set_span(const ScpiCommand& cmd) {
    if (cmd.board_id < 0 || cmd.dac_id < 0) {
        return "ERROR:Missing address";
    }

    DacDevice* dac = get_dac(cmd.board_id, cmd.dac_id);
    if (!dac) {
        return "ERROR:DAC not initialized";
    }

    if (cmd.type == ScpiCommandType::SET_ALL_SPAN) {
        // Set span for all channels
        for (uint8_t ch = 0; ch < dac->get_num_channels(); ch++) {
            dac->set_span(ch, static_cast<uint8_t>(cmd.int_value));
        }
    } else {
        // Need channel for single-channel span
        if (cmd.channel_id < 0) {
            return "ERROR:Missing channel";
        }
        dac->set_span(cmd.channel_id, static_cast<uint8_t>(cmd.int_value));
    }

    return "OK";
}

std::string BoardManager::execute_update(const ScpiCommand& cmd) {
    if (cmd.type == ScpiCommandType::UPDATE_ALL) {
        // Update all DACs on all boards
        for (uint8_t board = 0; board < NUM_BOARDS; board++) {
            if (current_dacs_[board][0]) current_dacs_[board][0]->update_all();
            if (current_dacs_[board][1]) current_dacs_[board][1]->update_all();
            if (voltage_dacs_[board]) voltage_dacs_[board]->update_all();
        }
        spi_.pulse_ldac();  // Global LDAC pulse
        return "OK";
    }

    if (cmd.board_id < 0 || cmd.dac_id < 0) {
        return "ERROR:Missing address";
    }

    DacDevice* dac = get_dac(cmd.board_id, cmd.dac_id);
    if (!dac) {
        return "ERROR:DAC not initialized";
    }

    dac->update_all();
    return "OK";
}

std::string BoardManager::execute_power_down(const ScpiCommand& cmd) {
    if (cmd.board_id < 0 || cmd.dac_id < 0) {
        return "ERROR:Missing address";
    }

    DacDevice* dac = get_dac(cmd.board_id, cmd.dac_id);
    if (!dac) {
        return "ERROR:DAC not initialized";
    }

    if (cmd.type == ScpiCommandType::POWER_DOWN_CHIP) {
        dac->power_down_chip();
    } else {
        if (cmd.channel_id < 0) {
            return "ERROR:Missing channel";
        }
        dac->power_down(cmd.channel_id);
    }

    return "OK";
}

std::string BoardManager::execute_get_resolution(const ScpiCommand& cmd) {
    if (cmd.board_id < 0 || cmd.dac_id < 0) {
        return "ERROR:Missing address";
    }

    DacDevice* dac = get_dac(cmd.board_id, cmd.dac_id);
    if (!dac) {
        return "ERROR:DAC not initialized";
    }

    char buf[16];
    snprintf(buf, sizeof(buf), "%u", dac->get_resolution());
    return buf;
}

std::string BoardManager::execute_set_resolution(const ScpiCommand& cmd) {
    if (cmd.board_id < 0 || cmd.dac_id < 0) {
        return "ERROR:Missing address";
    }

    if (cmd.board_id >= NUM_BOARDS || cmd.dac_id >= DACS_PER_BOARD) {
        return "ERROR:Invalid board/DAC";
    }

    uint8_t new_res = static_cast<uint8_t>(cmd.int_value);
    set_resolution(cmd.board_id, cmd.dac_id, new_res);

    // Re-initialize the specific DAC with new resolution
    if (cmd.dac_id < 2) {
        uint8_t idx = cmd.board_id * 2 + cmd.dac_id;
        current_dac_storage_[idx].setup(&spi_, cmd.board_id, cmd.dac_id, new_res);
        current_dacs_[cmd.board_id][cmd.dac_id]->init();
    } else {
        voltage_dac_storage_[cmd.board_id].setup(&spi_, cmd.board_id, 2, new_res);
        voltage_dacs_[cmd.board_id]->init();
    }

    return "OK";
}

void BoardManager::set_serial_number(uint8_t board, const std::string& serial) {
    if (board >= NUM_BOARDS) return;
    strncpy(serial_numbers_[board], serial.c_str(), SERIAL_NUMBER_MAX_LEN - 1);
    serial_numbers_[board][SERIAL_NUMBER_MAX_LEN - 1] = '\0';
}

std::string BoardManager::get_serial_number(uint8_t board) const {
    if (board >= NUM_BOARDS) return "";
    return serial_numbers_[board];
}

void BoardManager::set_cal_gain(uint8_t board, uint8_t dac, uint8_t channel, float gain) {
    if (board >= NUM_BOARDS || dac >= DACS_PER_BOARD || channel >= MAX_CHANNELS_PER_DAC) return;
    calibration_[board][dac][channel].gain = gain;
}

float BoardManager::get_cal_gain(uint8_t board, uint8_t dac, uint8_t channel) const {
    if (board >= NUM_BOARDS || dac >= DACS_PER_BOARD || channel >= MAX_CHANNELS_PER_DAC) return 1.0f;
    return calibration_[board][dac][channel].gain;
}

void BoardManager::set_cal_offset(uint8_t board, uint8_t dac, uint8_t channel, float offset) {
    if (board >= NUM_BOARDS || dac >= DACS_PER_BOARD || channel >= MAX_CHANNELS_PER_DAC) return;
    calibration_[board][dac][channel].offset = offset;
}

float BoardManager::get_cal_offset(uint8_t board, uint8_t dac, uint8_t channel) const {
    if (board >= NUM_BOARDS || dac >= DACS_PER_BOARD || channel >= MAX_CHANNELS_PER_DAC) return 0.0f;
    return calibration_[board][dac][channel].offset;
}

void BoardManager::set_cal_enable(uint8_t board, uint8_t dac, uint8_t channel, bool enable) {
    if (board >= NUM_BOARDS || dac >= DACS_PER_BOARD || channel >= MAX_CHANNELS_PER_DAC) return;
    calibration_[board][dac][channel].enabled = enable;
}

bool BoardManager::get_cal_enable(uint8_t board, uint8_t dac, uint8_t channel) const {
    if (board >= NUM_BOARDS || dac >= DACS_PER_BOARD || channel >= MAX_CHANNELS_PER_DAC) return false;
    return calibration_[board][dac][channel].enabled;
}

const ChannelCalibration* BoardManager::get_calibration(uint8_t board, uint8_t dac, uint8_t channel) const {
    if (board >= NUM_BOARDS || dac >= DACS_PER_BOARD || channel >= MAX_CHANNELS_PER_DAC) return nullptr;
    return &calibration_[board][dac][channel];
}

void BoardManager::clear_all_calibration() {
    for (uint8_t board = 0; board < NUM_BOARDS; board++) {
        serial_numbers_[board][0] = '\0';
        for (uint8_t dac = 0; dac < DACS_PER_BOARD; dac++) {
            for (uint8_t ch = 0; ch < MAX_CHANNELS_PER_DAC; ch++) {
                calibration_[board][dac][ch].gain = 1.0f;
                calibration_[board][dac][ch].offset = 0.0f;
                calibration_[board][dac][ch].enabled = false;
            }
        }
    }
}

std::string BoardManager::export_calibration_data() const {
    // Export calibration data in a compact format
    // Format: BOARD<n>:SN=<serial>;DAC<m>:CH<c>:G=<gain>,O=<offset>,E=<0|1>
    std::string result;
    char buf[128];

    for (uint8_t board = 0; board < NUM_BOARDS; board++) {
        // Output board header with serial number
        snprintf(buf, sizeof(buf), "BOARD%d:SN=%s\n", board, serial_numbers_[board]);
        result += buf;

        for (uint8_t dac = 0; dac < DACS_PER_BOARD; dac++) {
            uint8_t num_ch = (dac < 2) ? LTC2662::NUM_CHANNELS : LTC2664::NUM_CHANNELS;
            for (uint8_t ch = 0; ch < num_ch; ch++) {
                const ChannelCalibration& cal = calibration_[board][dac][ch];
                // Only output non-default calibrations
                if (cal.enabled || cal.gain != 1.0f || cal.offset != 0.0f) {
                    snprintf(buf, sizeof(buf), "  DAC%d:CH%d:G=%.6f,O=%.6f,E=%d\n",
                             dac, ch, cal.gain, cal.offset, cal.enabled ? 1 : 0);
                    result += buf;
                }
            }
        }
    }

    return result;
}

std::string BoardManager::execute_set_serial(const ScpiCommand& cmd) {
    if (cmd.board_id < 0 || cmd.board_id >= NUM_BOARDS) {
        return "ERROR:Invalid board";
    }
    set_serial_number(cmd.board_id, cmd.string_value);
    return "OK";
}

std::string BoardManager::execute_get_serial(const ScpiCommand& cmd) {
    if (cmd.board_id < 0 || cmd.board_id >= NUM_BOARDS) {
        return "ERROR:Invalid board";
    }
    std::string sn = get_serial_number(cmd.board_id);
    if (sn.empty()) {
        return "(not set)";
    }
    return sn;
}

std::string BoardManager::execute_set_cal_gain(const ScpiCommand& cmd) {
    if (cmd.board_id < 0 || cmd.dac_id < 0 || cmd.channel_id < 0) {
        return "ERROR:Missing address";
    }
    DacDevice* dac = get_dac(cmd.board_id, cmd.dac_id);
    if (!dac || cmd.channel_id >= dac->get_num_channels()) {
        return "ERROR:Invalid channel";
    }
    set_cal_gain(cmd.board_id, cmd.dac_id, cmd.channel_id, cmd.float_value);
    return "OK";
}

std::string BoardManager::execute_get_cal_gain(const ScpiCommand& cmd) {
    if (cmd.board_id < 0 || cmd.dac_id < 0 || cmd.channel_id < 0) {
        return "ERROR:Missing address";
    }
    DacDevice* dac = get_dac(cmd.board_id, cmd.dac_id);
    if (!dac || cmd.channel_id >= dac->get_num_channels()) {
        return "ERROR:Invalid channel";
    }
    char buf[32];
    snprintf(buf, sizeof(buf), "%.6f", get_cal_gain(cmd.board_id, cmd.dac_id, cmd.channel_id));
    return buf;
}

std::string BoardManager::execute_set_cal_offset(const ScpiCommand& cmd) {
    if (cmd.board_id < 0 || cmd.dac_id < 0 || cmd.channel_id < 0) {
        return "ERROR:Missing address";
    }
    DacDevice* dac = get_dac(cmd.board_id, cmd.dac_id);
    if (!dac || cmd.channel_id >= dac->get_num_channels()) {
        return "ERROR:Invalid channel";
    }
    set_cal_offset(cmd.board_id, cmd.dac_id, cmd.channel_id, cmd.float_value);
    return "OK";
}

std::string BoardManager::execute_get_cal_offset(const ScpiCommand& cmd) {
    if (cmd.board_id < 0 || cmd.dac_id < 0 || cmd.channel_id < 0) {
        return "ERROR:Missing address";
    }
    DacDevice* dac = get_dac(cmd.board_id, cmd.dac_id);
    if (!dac || cmd.channel_id >= dac->get_num_channels()) {
        return "ERROR:Invalid channel";
    }
    char buf[32];
    snprintf(buf, sizeof(buf), "%.6f", get_cal_offset(cmd.board_id, cmd.dac_id, cmd.channel_id));
    return buf;
}

std::string BoardManager::execute_set_cal_enable(const ScpiCommand& cmd) {
    if (cmd.board_id < 0 || cmd.dac_id < 0 || cmd.channel_id < 0) {
        return "ERROR:Missing address";
    }
    DacDevice* dac = get_dac(cmd.board_id, cmd.dac_id);
    if (!dac || cmd.channel_id >= dac->get_num_channels()) {
        return "ERROR:Invalid channel";
    }
    set_cal_enable(cmd.board_id, cmd.dac_id, cmd.channel_id, cmd.int_value != 0);
    return "OK";
}

std::string BoardManager::execute_get_cal_enable(const ScpiCommand& cmd) {
    if (cmd.board_id < 0 || cmd.dac_id < 0 || cmd.channel_id < 0) {
        return "ERROR:Missing address";
    }
    DacDevice* dac = get_dac(cmd.board_id, cmd.dac_id);
    if (!dac || cmd.channel_id >= dac->get_num_channels()) {
        return "ERROR:Invalid channel";
    }
    return get_cal_enable(cmd.board_id, cmd.dac_id, cmd.channel_id) ? "1" : "0";
}

std::string BoardManager::execute_cal_data_query() {
    return export_calibration_data();
}

std::string BoardManager::execute_cal_clear() {
    clear_all_calibration();
    // Also erase from flash
    CalStorage::erase_flash();
    return "OK";
}

std::string BoardManager::execute_cal_save() {
    if (CalStorage::save_to_flash(*this)) {
        return "OK";
    }
    return "ERROR:Flash write failed";
}

std::string BoardManager::execute_cal_load() {
    if (CalStorage::load_from_flash(*this)) {
        return "OK";
    }
    return "ERROR:No valid calibration data";
}

std::string BoardManager::execute(const ScpiCommand& cmd) {
    if (!cmd.valid) {
        return "ERROR:" + cmd.error_msg;
    }

    switch (cmd.type) {
        case ScpiCommandType::IDN_QUERY:
            return execute_idn();

        case ScpiCommandType::RST:
            reset_all();
            return "OK";

        case ScpiCommandType::FAULT_QUERY:
            return execute_fault_query();

        case ScpiCommandType::SET_VOLTAGE:
            return execute_set_voltage(cmd);

        case ScpiCommandType::SET_CURRENT:
            return execute_set_current(cmd);

        case ScpiCommandType::SET_CODE:
            return execute_set_code(cmd);

        case ScpiCommandType::SET_SPAN:
        case ScpiCommandType::SET_ALL_SPAN:
            return execute_set_span(cmd);

        case ScpiCommandType::UPDATE:
        case ScpiCommandType::UPDATE_ALL:
            return execute_update(cmd);

        case ScpiCommandType::POWER_DOWN:
        case ScpiCommandType::POWER_DOWN_CHIP:
            return execute_power_down(cmd);

        case ScpiCommandType::GET_RESOLUTION:
            return execute_get_resolution(cmd);

        case ScpiCommandType::SET_RESOLUTION:
            return execute_set_resolution(cmd);

        case ScpiCommandType::PULSE_LDAC:
            spi_.pulse_ldac();
            return "OK";

        case ScpiCommandType::SYST_ERR_QUERY:
            return "0,\"No error\"";  // TODO: Implement error queue

        case ScpiCommandType::GET_VOLTAGE:
        case ScpiCommandType::GET_CURRENT:
            return "ERROR:Query not implemented";

        // Calibration commands
        case ScpiCommandType::SET_SERIAL:
            return execute_set_serial(cmd);

        case ScpiCommandType::GET_SERIAL:
            return execute_get_serial(cmd);

        case ScpiCommandType::SET_CAL_GAIN:
            return execute_set_cal_gain(cmd);

        case ScpiCommandType::GET_CAL_GAIN:
            return execute_get_cal_gain(cmd);

        case ScpiCommandType::SET_CAL_OFFSET:
            return execute_set_cal_offset(cmd);

        case ScpiCommandType::GET_CAL_OFFSET:
            return execute_get_cal_offset(cmd);

        case ScpiCommandType::SET_CAL_ENABLE:
            return execute_set_cal_enable(cmd);

        case ScpiCommandType::GET_CAL_ENABLE:
            return execute_get_cal_enable(cmd);

        case ScpiCommandType::CAL_DATA_QUERY:
            return execute_cal_data_query();

        case ScpiCommandType::CAL_CLEAR:
            return execute_cal_clear();

        case ScpiCommandType::CAL_SAVE:
            return execute_cal_save();

        case ScpiCommandType::CAL_LOAD:
            return execute_cal_load();

        default:
            return "ERROR:Unknown command";
    }
}
