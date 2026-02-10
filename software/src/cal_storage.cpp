#include "cal_storage.hpp"
#include <cstring>
#include <cstdio>

// Pico SDK includes for flash operations
#include "hardware/flash.h"
#include "hardware/sync.h"
#include "pico/stdlib.h"

namespace CalStorage {

// Flash is memory-mapped at XIP_BASE
// To read flash, access (XIP_BASE + offset)
#define FLASH_TARGET_OFFSET CAL_FLASH_OFFSET

// Calculate CRC-16 (CCITT polynomial 0x1021)
uint16_t calculate_crc16(const uint8_t* data, size_t length) {
    uint16_t crc = 0xFFFF;

    for (size_t i = 0; i < length; i++) {
        crc ^= (static_cast<uint16_t>(data[i]) << 8);
        for (int j = 0; j < 8; j++) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ 0x1021;
            } else {
                crc <<= 1;
            }
        }
    }

    return crc;
}

bool has_valid_data() {
    // Read the header from flash
    const FlashCalibrationData* flash_data =
        reinterpret_cast<const FlashCalibrationData*>(XIP_BASE + FLASH_TARGET_OFFSET);

    // Check magic number
    if (flash_data->magic != CAL_MAGIC) {
        return false;
    }

    // Check version (must match current version)
    if (flash_data->version != CAL_VERSION) {
        return false;
    }

    // Calculate CRC over data portion (after header)
    const uint8_t* data_start = reinterpret_cast<const uint8_t*>(&flash_data->serial_numbers);
    size_t data_size = sizeof(FlashCalibrationData) - offsetof(FlashCalibrationData, serial_numbers);
    uint16_t calculated_crc = calculate_crc16(data_start, data_size);

    return (calculated_crc == flash_data->checksum);
}

bool save_to_flash(const BoardManager& manager) {
    // Prepare calibration data structure
    FlashCalibrationData cal_data;
    memset(&cal_data, 0xFF, sizeof(cal_data));  // Fill with 0xFF (erased flash state)

    // Set header
    cal_data.magic = CAL_MAGIC;
    cal_data.version = CAL_VERSION;

    // Copy serial numbers
    for (uint8_t board = 0; board < NUM_BOARDS; board++) {
        std::string sn = manager.get_serial_number(board);
        strncpy(cal_data.serial_numbers[board], sn.c_str(), SERIAL_NUMBER_MAX_LEN - 1);
        cal_data.serial_numbers[board][SERIAL_NUMBER_MAX_LEN - 1] = '\0';
    }

    // Copy calibration data
    for (uint8_t board = 0; board < NUM_BOARDS; board++) {
        for (uint8_t dac = 0; dac < DACS_PER_BOARD; dac++) {
            for (uint8_t ch = 0; ch < MAX_CHANNELS_PER_DAC; ch++) {
                const ChannelCalibration* cal = manager.get_calibration(board, dac, ch);
                if (cal) {
                    cal_data.channels[board][dac][ch].gain = cal->gain;
                    cal_data.channels[board][dac][ch].offset = cal->offset;
                    cal_data.channels[board][dac][ch].enabled = cal->enabled ? 1 : 0;
                } else {
                    cal_data.channels[board][dac][ch].gain = 1.0f;
                    cal_data.channels[board][dac][ch].offset = 0.0f;
                    cal_data.channels[board][dac][ch].enabled = 0;
                }
            }
        }
    }

    // Calculate CRC over data portion
    const uint8_t* data_start = reinterpret_cast<const uint8_t*>(&cal_data.serial_numbers);
    size_t data_size = sizeof(FlashCalibrationData) - offsetof(FlashCalibrationData, serial_numbers);
    cal_data.checksum = calculate_crc16(data_start, data_size);

    // Disable interrupts during flash operations
    uint32_t interrupts = save_and_disable_interrupts();

    // Erase the sector first (required before writing)
    flash_range_erase(FLASH_TARGET_OFFSET, FLASH_SECTOR_SIZE);

    // Write the data (must be aligned to page size)
    // Write in chunks of FLASH_PAGE_SIZE
    const uint8_t* src = reinterpret_cast<const uint8_t*>(&cal_data);
    size_t bytes_to_write = sizeof(FlashCalibrationData);

    // Round up to page size
    size_t pages = (bytes_to_write + FLASH_PAGE_SIZE - 1) / FLASH_PAGE_SIZE;
    size_t write_size = pages * FLASH_PAGE_SIZE;

    // Create page-aligned buffer
    uint8_t write_buffer[write_size];
    memset(write_buffer, 0xFF, write_size);
    memcpy(write_buffer, src, bytes_to_write);

    flash_range_program(FLASH_TARGET_OFFSET, write_buffer, write_size);

    // Re-enable interrupts
    restore_interrupts(interrupts);

    // Verify write by checking CRC
    return has_valid_data();
}

bool load_from_flash(BoardManager& manager) {
    // Check if valid data exists
    if (!has_valid_data()) {
        printf("[CAL] No valid calibration data in flash\r\n");
        return false;
    }

    // Read from flash
    const FlashCalibrationData* flash_data =
        reinterpret_cast<const FlashCalibrationData*>(XIP_BASE + FLASH_TARGET_OFFSET);

    // Load serial numbers
    for (uint8_t board = 0; board < NUM_BOARDS; board++) {
        manager.set_serial_number(board, flash_data->serial_numbers[board]);
    }

    // Load calibration data
    for (uint8_t board = 0; board < NUM_BOARDS; board++) {
        for (uint8_t dac = 0; dac < DACS_PER_BOARD; dac++) {
            for (uint8_t ch = 0; ch < MAX_CHANNELS_PER_DAC; ch++) {
                const auto& cal = flash_data->channels[board][dac][ch];
                manager.set_cal_gain(board, dac, ch, cal.gain);
                manager.set_cal_offset(board, dac, ch, cal.offset);
                manager.set_cal_enable(board, dac, ch, cal.enabled != 0);
            }
        }
    }

    printf("[CAL] Loaded calibration data from flash\r\n");
    return true;
}

void erase_flash() {
    uint32_t interrupts = save_and_disable_interrupts();
    flash_range_erase(FLASH_TARGET_OFFSET, FLASH_SECTOR_SIZE);
    restore_interrupts(interrupts);
    printf("[CAL] Calibration data erased from flash\r\n");
}

}  // namespace CalStorage
