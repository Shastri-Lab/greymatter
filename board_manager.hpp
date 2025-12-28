#ifndef BOARD_MANAGER_HPP
#define BOARD_MANAGER_HPP

#include <array>
#include <string>
#include <memory>
#include <cstdio>

#include "dac_device.hpp"
#include "spi_manager.hpp"
#include "scpi_parser.hpp"
#include "ltc2662.hpp"
#include "ltc2664.hpp"

// Board configuration: 8 boards, each with 3 DACs
// DAC 0: LTC2662 (5-channel current DAC)
// DAC 1: LTC2662 (5-channel current DAC)
// DAC 2: LTC2664 (4-channel voltage DAC)
constexpr uint8_t NUM_BOARDS = 8;
constexpr uint8_t DACS_PER_BOARD = 3;

// Default DAC resolutions (can be overridden per-board)
// Set to 12 for LTC2662-12/LTC2664-12, 16 for LTC2662-16/LTC2664-16
constexpr uint8_t DEFAULT_CURRENT_DAC_RESOLUTION = 16;
constexpr uint8_t DEFAULT_VOLTAGE_DAC_RESOLUTION = 12;

// Manages 8 daughter boards, each with 3 DACs connected via SPI
class BoardManager {
public:
    BoardManager(SpiManager& spi);

    // Initialize all DACs on all boards
    void init_all();

    // Execute a parsed SCPI command
    // Returns response string (empty for commands, value for queries)
    std::string execute(const ScpiCommand& cmd);

    // Reset all DACs to power-on state
    void reset_all();

    // Get DAC device by board/dac index
    DacDevice* get_dac(uint8_t board, uint8_t dac);

    // Get the current DAC type (for SCPI routing)
    // Returns 0 for LTC2662 (current DAC), 1 for LTC2664 (voltage DAC)
    uint8_t get_dac_type(uint8_t board, uint8_t dac);

    // Set resolution for a specific DAC (12 or 16 bits)
    // Must be called before init_all() or will require re-initialization
    void set_resolution(uint8_t board, uint8_t dac, uint8_t resolution_bits);

    // Get resolution for a specific DAC
    uint8_t get_resolution(uint8_t board, uint8_t dac);

private:
    SpiManager& spi_;
    ScpiParser parser_;

    // DAC storage: 8 boards x 3 DACs
    // Using raw pointers for embedded (unique_ptr has overhead)
    LTC2662* current_dacs_[NUM_BOARDS][2];  // 2 LTC2662 per board (DAC 0, 1)
    LTC2664* voltage_dacs_[NUM_BOARDS];     // 1 LTC2664 per board (DAC 2)

    // Actual storage
    LTC2662 current_dac_storage_[NUM_BOARDS * 2];
    LTC2664 voltage_dac_storage_[NUM_BOARDS];

    // Resolution configuration per DAC: [board][dac]
    uint8_t resolution_[NUM_BOARDS][DACS_PER_BOARD];

    // Execute specific command types
    std::string execute_idn();
    std::string execute_fault_query();
    std::string execute_set_voltage(const ScpiCommand& cmd);
    std::string execute_set_current(const ScpiCommand& cmd);
    std::string execute_set_code(const ScpiCommand& cmd);
    std::string execute_set_span(const ScpiCommand& cmd);
    std::string execute_update(const ScpiCommand& cmd);
    std::string execute_power_down(const ScpiCommand& cmd);
    std::string execute_get_resolution(const ScpiCommand& cmd);
    std::string execute_set_resolution(const ScpiCommand& cmd);
};

#endif // BOARD_MANAGER_HPP
