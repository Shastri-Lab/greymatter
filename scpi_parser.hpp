#ifndef SCPI_PARSER_HPP
#define SCPI_PARSER_HPP

#include <string>
#include <vector>
#include <cstdint>

// SCPI command types
enum class ScpiCommandType {
    UNKNOWN,
    // IEEE 488.2 common commands
    IDN_QUERY,       // *IDN?
    RST,             // *RST
    // Board/DAC commands
    SET_VOLTAGE,     // BOARD<n>:DAC<m>:CH<c>:VOLT <value>
    GET_VOLTAGE,     // BOARD<n>:DAC<m>:CH<c>:VOLT?
    SET_CURRENT,     // BOARD<n>:DAC<m>:CH<c>:CURR <value>
    GET_CURRENT,     // BOARD<n>:DAC<m>:CH<c>:CURR?
    SET_CODE,        // BOARD<n>:DAC<m>:CH<c>:CODE <value>
    SET_SPAN,        // BOARD<n>:DAC<m>:SPAN <value>
    SET_ALL_SPAN,    // BOARD<n>:DAC<m>:SPAN:ALL <value>
    UPDATE,          // BOARD<n>:DAC<m>:UPDATE
    UPDATE_ALL,      // UPDATE:ALL
    POWER_DOWN,      // BOARD<n>:DAC<m>:CH<c>:PDOWN
    POWER_DOWN_CHIP, // BOARD<n>:DAC<m>:PDOWN
    SET_RESOLUTION,  // BOARD<n>:DAC<m>:RES <12|16>
    GET_RESOLUTION,  // BOARD<n>:DAC<m>:RES?
    // Calibration commands
    SET_CAL_GAIN,    // BOARD<n>:DAC<m>:CH<c>:CAL:GAIN <value>
    GET_CAL_GAIN,    // BOARD<n>:DAC<m>:CH<c>:CAL:GAIN?
    SET_CAL_OFFSET,  // BOARD<n>:DAC<m>:CH<c>:CAL:OFFS <value>
    GET_CAL_OFFSET,  // BOARD<n>:DAC<m>:CH<c>:CAL:OFFS?
    SET_CAL_ENABLE,  // BOARD<n>:DAC<m>:CH<c>:CAL:EN <0|1>
    GET_CAL_ENABLE,  // BOARD<n>:DAC<m>:CH<c>:CAL:EN?
    SET_SERIAL,      // BOARD<n>:SN <string>
    GET_SERIAL,      // BOARD<n>:SN?
    CAL_DATA_QUERY,  // CAL:DATA? - Export all calibration data
    CAL_CLEAR,       // CAL:CLEAR - Clear all calibration data
    CAL_SAVE,        // CAL:SAVE - Save calibration to flash
    CAL_LOAD,        // CAL:LOAD - Load calibration from flash
    // System commands
    FAULT_QUERY,     // FAULT?
    SYST_ERR_QUERY,  // SYST:ERR?
    PULSE_LDAC,      // LDAC

#ifdef DEBUG_SPI_MODE
    // Debug commands (only available in debug build)
    DEBUG_TRACE,         // DEBUG:TRACE <level>  - Set trace verbosity (0-3)
    DEBUG_STEP_MODE,     // DEBUG:STEP:MODE <0|1> - Enable/disable step mode
    DEBUG_STEP,          // DEBUG:STEP - Advance one step
    DEBUG_LOOPBACK,      // DEBUG:LOOPBACK <0|1> - Enable/disable loopback pins
    DEBUG_STATUS,        // DEBUG:STATUS? - Query debug mode status
    DEBUG_TEST_BYTE,     // DEBUG:TEST:BYTE <hex> - Send a test byte and show trace
    DEBUG_TEST_EXPANDER, // DEBUG:TEST:EXPANDER <addr> - Test IO expander communication
#endif
};

// Parsed SCPI command structure
struct ScpiCommand {
    ScpiCommandType type = ScpiCommandType::UNKNOWN;
    bool is_query = false;

    // Addressing (for board/DAC commands)
    int8_t board_id = -1;     // 0-7, -1 if not specified
    int8_t dac_id = -1;       // 0-2, -1 if not specified
    int8_t channel_id = -1;   // 0-4, -1 if not specified

    // Value (for set commands)
    float float_value = 0.0f;
    uint16_t int_value = 0;
    bool has_float = false;
    bool has_int = false;

    // String value (for serial number)
    std::string string_value;
    bool has_string = false;

    // Error state
    bool valid = false;
    std::string error_msg;
};

// SCPI Parser - parses incoming commands and produces structured command objects
class ScpiParser {
public:
    // Parse a single SCPI command line
    ScpiCommand parse(const char* line);
    ScpiCommand parse(const std::string& line);

private:
    // Parse helpers
    bool parse_common_command(const char* cmd, ScpiCommand& result);
    bool parse_board_command(const char* cmd, ScpiCommand& result);
    bool parse_system_command(const char* cmd, ScpiCommand& result);

#ifdef DEBUG_SPI_MODE
    bool parse_debug_command(const char* cmd, ScpiCommand& result);
#endif

    // Extract numeric index from string like "BOARD3" -> 3
    int extract_index(const char* str, const char* prefix);

    // Skip whitespace and return pointer to next non-whitespace
    const char* skip_whitespace(const char* str);

    // Parse numeric value
    bool parse_float(const char* str, float& value);
    bool parse_int(const char* str, uint16_t& value);
};

// Legacy compatibility
struct scpi_command_t {
    std::string cmd;
    std::vector<std::string> args;
};

#endif // SCPI_PARSER_HPP
