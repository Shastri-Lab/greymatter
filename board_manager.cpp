#include "board_manager.hpp"
#include "utils.hpp"
#include <cstdio>

BoardManager::BoardManager(SpiManager& spi) : spi_(spi) {
    // Initialize DAC pointers and storage
    // Each board has: DAC0=LTC2662, DAC1=LTC2662, DAC2=LTC2664
    for (uint8_t board = 0; board < NUM_BOARDS; board++) {
        // LTC2662 current DACs (device_id 0 and 1)
        current_dacs_[board][0] = nullptr;
        current_dacs_[board][1] = nullptr;
        voltage_dacs_[board] = nullptr;
    }
}

void BoardManager::init_all() {
    // Setup and initialize all DAC devices
    for (uint8_t board = 0; board < NUM_BOARDS; board++) {
        // Setup LTC2662 current DACs at device_id 0 and 1
        uint8_t idx0 = board * 2;
        uint8_t idx1 = board * 2 + 1;

        current_dac_storage_[idx0].setup(&spi_, board, 0);
        current_dac_storage_[idx1].setup(&spi_, board, 1);
        current_dacs_[board][0] = &current_dac_storage_[idx0];
        current_dacs_[board][1] = &current_dac_storage_[idx1];

        // Setup LTC2664 voltage DAC at device_id 2
        voltage_dac_storage_[board].setup(&spi_, board, 2);
        voltage_dacs_[board] = &voltage_dac_storage_[board];

        // Initialize each DAC
        current_dacs_[board][0]->init();
        current_dacs_[board][1]->init();
        voltage_dacs_[board]->init();
    }
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

std::string BoardManager::execute_idn() {
    return "GreyMatter,DAC Controller,001,0.1";
}

std::string BoardManager::execute_fault_query() {
    if (spi_.is_fault_active()) {
        uint32_t faults = spi_.io_expander().read_faults();
        char buf[32];
        snprintf(buf, sizeof(buf), "FAULT:0x%06lX", faults);
        return buf;
    }
    return "OK";
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

    dac->set_voltage(cmd.channel_id, cmd.float_value);
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

    dac->set_current_ma(cmd.channel_id, cmd.float_value);
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

        case ScpiCommandType::PULSE_LDAC:
            spi_.pulse_ldac();
            return "OK";

        case ScpiCommandType::SYST_ERR_QUERY:
            return "0,\"No error\"";  // TODO: Implement error queue

        case ScpiCommandType::GET_VOLTAGE:
        case ScpiCommandType::GET_CURRENT:
            return "ERROR:Query not implemented";

        default:
            return "ERROR:Unknown command";
    }
}
