#ifndef CAL_STORAGE_HPP
#define CAL_STORAGE_HPP

#include <cstdint>
#include "board_manager.hpp"

// Flash storage for calibration data on RP2350
// Uses the last 4KB sector of 2MB flash to avoid program code

namespace CalStorage {

// Flash configuration for RP2350 (Pico 2)
constexpr uint32_t FLASH_SIZE = 4 * 1024 * 1024;        // 2MB flash
constexpr uint32_t FLASH_SECTOR_SIZE = 4096;            // 4KB erase sector
constexpr uint32_t FLASH_PAGE_SIZE = 256;               // 256-byte write page

// Use the last sector for calibration storage
// This leaves plenty of room for program code
constexpr uint32_t CAL_FLASH_OFFSET = FLASH_SIZE - FLASH_SECTOR_SIZE;  // 0x3FF000

// Controller serial number stored in the sector before calibration
constexpr uint32_t CONTROLLER_FLASH_OFFSET = FLASH_SIZE - 2 * FLASH_SECTOR_SIZE;  // 0x3FE000

// Magic number to identify valid calibration data
constexpr uint32_t CAL_MAGIC = 0x47524D43;  // "GRMC" (greymatter Calibration)
constexpr uint16_t CAL_VERSION = 1;

// Magic number for controller serial number data
constexpr uint32_t CONTROLLER_MAGIC = 0x474D4353;  // "GMCS" (GreyMatter Controller Serial)
constexpr uint16_t CONTROLLER_VERSION = 1;

// Calibration data structure stored in flash
// Total size must fit in one sector (4KB)
struct __attribute__((packed)) FlashCalibrationData {
    // Header (8 bytes)
    uint32_t magic;              // Magic number for validation
    uint16_t version;            // Data format version
    uint16_t checksum;           // CRC-16 of data following header

    // Board serial numbers (256 bytes)
    char serial_numbers[NUM_BOARDS][SERIAL_NUMBER_MAX_LEN];

    // Calibration data for all channels
    // 8 boards × 3 DACs × 5 channels = 120 entries
    // Each entry: gain (4) + offset (4) + enabled (1) = 9 bytes
    // Total: 1080 bytes
    struct __attribute__((packed)) ChannelCalData {
        float gain;
        float offset;
        uint8_t enabled;
    } channels[NUM_BOARDS][DACS_PER_BOARD][MAX_CHANNELS_PER_DAC];

    // Padding to align to page boundary (optional, for future expansion)
    uint8_t reserved[256];
};

// Verify structure fits in one sector
static_assert(sizeof(FlashCalibrationData) <= FLASH_SECTOR_SIZE,
              "Calibration data exceeds flash sector size");

// Controller serial number structure stored in flash
struct __attribute__((packed)) FlashControllerData {
    // Header (8 bytes)
    uint32_t magic;
    uint16_t version;
    uint16_t checksum;

    // Serial number (32 bytes)
    char serial_number[SERIAL_NUMBER_MAX_LEN];
};

static_assert(sizeof(FlashControllerData) <= FLASH_SECTOR_SIZE,
              "Controller data exceeds flash sector size");

// Calculate CRC-16 (CCITT) for data integrity
uint16_t calculate_crc16(const uint8_t* data, size_t length);

// Save calibration data to flash
// Returns true on success
bool save_to_flash(const BoardManager& manager);

// Load calibration data from flash
// Returns true if valid data was found and loaded
bool load_from_flash(BoardManager& manager);

// Check if valid calibration data exists in flash
bool has_valid_data();

// Erase calibration data from flash
void erase_flash();

// Save controller serial number to flash (separate sector from calibration)
bool save_controller_sn(const char* sn);

// Load controller serial number from flash
// Returns true if valid data was found
bool load_controller_sn(char* sn, size_t max_len);

}  // namespace CalStorage

#endif // CAL_STORAGE_HPP
