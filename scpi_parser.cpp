#include "scpi_parser.hpp"
#include "utils.hpp"
#include <cstring>
#include <cctype>
#include <cstdlib>

// Case-insensitive string comparison (portable replacement for strncasecmp)
static int strncasecmp_local(const char* s1, const char* s2, size_t n) {
    for (size_t i = 0; i < n; i++) {
        int c1 = std::toupper(static_cast<unsigned char>(s1[i]));
        int c2 = std::toupper(static_cast<unsigned char>(s2[i]));
        if (c1 != c2) return c1 - c2;
        if (c1 == '\0') return 0;
    }
    return 0;
}

const char* ScpiParser::skip_whitespace(const char* str) {
    while (*str && std::isspace(*str)) str++;
    return str;
}

bool ScpiParser::parse_float(const char* str, float& value) {
    str = skip_whitespace(str);
    char* end;
    value = std::strtof(str, &end);
    return end != str;
}

bool ScpiParser::parse_int(const char* str, uint16_t& value) {
    str = skip_whitespace(str);
    char* end;
    long v = std::strtol(str, &end, 0);  // Auto-detect hex (0x) or decimal
    if (end == str) return false;
    if (v < 0 || v > 65535) return false;
    value = static_cast<uint16_t>(v);
    return true;
}

int ScpiParser::extract_index(const char* str, const char* prefix) {
    size_t prefix_len = std::strlen(prefix);

    // Case-insensitive prefix match
    for (size_t i = 0; i < prefix_len; i++) {
        if (std::toupper(str[i]) != std::toupper(prefix[i])) {
            return -1;
        }
    }

    // Extract digit after prefix
    if (!std::isdigit(str[prefix_len])) {
        return -1;
    }

    return str[prefix_len] - '0';
}

bool ScpiParser::parse_common_command(const char* cmd, ScpiCommand& result) {
    // IEEE 488.2 common commands start with *
    if (cmd[0] != '*') return false;

    cmd++;  // Skip *

    if (strncasecmp_local(cmd, "IDN?", 4) == 0) {
        result.type = ScpiCommandType::IDN_QUERY;
        result.is_query = true;
        result.valid = true;
        return true;
    }

    if (strncasecmp_local(cmd, "RST", 3) == 0) {
        result.type = ScpiCommandType::RST;
        result.valid = true;
        return true;
    }

    return false;
}

bool ScpiParser::parse_system_command(const char* cmd, ScpiCommand& result) {
    // FAULT?
    if (strncasecmp_local(cmd, "FAULT?", 6) == 0) {
        result.type = ScpiCommandType::FAULT_QUERY;
        result.is_query = true;
        result.valid = true;
        return true;
    }

    // LDAC
    if (strncasecmp_local(cmd, "LDAC", 4) == 0) {
        result.type = ScpiCommandType::PULSE_LDAC;
        result.valid = true;
        return true;
    }

    // UPDATE:ALL
    if (strncasecmp_local(cmd, "UPDATE:ALL", 10) == 0) {
        result.type = ScpiCommandType::UPDATE_ALL;
        result.valid = true;
        return true;
    }

    // SYST:ERR?
    if (strncasecmp_local(cmd, "SYST:ERR?", 9) == 0) {
        result.type = ScpiCommandType::SYST_ERR_QUERY;
        result.is_query = true;
        result.valid = true;
        return true;
    }

    // CAL:DATA?
    if (strncasecmp_local(cmd, "CAL:DATA?", 9) == 0) {
        result.type = ScpiCommandType::CAL_DATA_QUERY;
        result.is_query = true;
        result.valid = true;
        return true;
    }

    // CAL:CLEAR
    if (strncasecmp_local(cmd, "CAL:CLEAR", 9) == 0) {
        result.type = ScpiCommandType::CAL_CLEAR;
        result.valid = true;
        return true;
    }

    // CAL:SAVE
    if (strncasecmp_local(cmd, "CAL:SAVE", 8) == 0) {
        result.type = ScpiCommandType::CAL_SAVE;
        result.valid = true;
        return true;
    }

    // CAL:LOAD
    if (strncasecmp_local(cmd, "CAL:LOAD", 8) == 0) {
        result.type = ScpiCommandType::CAL_LOAD;
        result.valid = true;
        return true;
    }

    return false;
}

bool ScpiParser::parse_board_command(const char* cmd, ScpiCommand& result) {
    // BOARD<n>:...
    if (strncasecmp_local(cmd, "BOARD", 5) != 0) {
        return false;
    }

    // Extract board number
    int board = extract_index(cmd, "BOARD");
    if (board < 0 || board > 7) { // TODO: hard-coded 8 boards; should be a global parameter
        result.error_msg = "Invalid board number (0-7)";
        return false;
    }
    result.board_id = board;

    // Find next colon
    const char* p = std::strchr(cmd, ':');
    if (!p) {
        result.error_msg = "Expected :DAC or :SN after BOARD";
        return false;
    }
    p++;  // Skip :

    // Check for BOARD<n>:SN (serial number)
    if (strncasecmp_local(p, "SN", 2) == 0) {
        p += 2;
        if (*p == '?') {
            result.type = ScpiCommandType::GET_SERIAL;
            result.is_query = true;
            result.valid = true;
        } else {
            result.type = ScpiCommandType::SET_SERIAL;
            p = skip_whitespace(p);
            // Read the rest as serial number string (up to newline/null)
            std::string sn;
            while (*p && *p != '\n' && *p != '\r') {
                sn += *p++;
            }
            // Trim trailing whitespace
            while (!sn.empty() && std::isspace(sn.back())) {
                sn.pop_back();
            }
            if (sn.empty()) {
                result.error_msg = "Serial number required";
                return false;
            }
            result.string_value = sn;
            result.has_string = true;
            result.valid = true;
        }
        return true;
    }

    // DAC<n>:...
    if (strncasecmp_local(p, "DAC", 3) != 0) {
        result.error_msg = "Expected DAC<n>";
        return false;
    }

    int dac = extract_index(p, "DAC");
    if (dac < 0 || dac > 2) {
        result.error_msg = "Invalid DAC number (0-2)";
        return false;
    }
    result.dac_id = dac;

    // Find next colon
    p = std::strchr(p, ':');
    if (!p) {
        result.error_msg = "Expected command after DAC";
        return false;
    }
    p++;  // Skip :

    // Parse subcommand: CH<n>:VOLT, CH<n>:CURR, CH<n>:CODE, SPAN, UPDATE, PDOWN
    if (strncasecmp_local(p, "CH", 2) == 0) {
        // CH<n>:...
        int ch = extract_index(p, "CH");
        if (ch < 0 || ch > 4) {
            result.error_msg = "Invalid channel number (0-4)";
            return false;
        }
        result.channel_id = ch;

        p = std::strchr(p, ':');
        if (!p) {
            result.error_msg = "Expected command after CH";
            return false;
        }
        p++;  // Skip :

        // VOLT, CURR, CODE, PDOWN
        if (strncasecmp_local(p, "VOLT", 4) == 0) {
            p += 4;
            if (*p == '?') {
                result.type = ScpiCommandType::GET_VOLTAGE;
                result.is_query = true;
                result.valid = true;
            } else {
                result.type = ScpiCommandType::SET_VOLTAGE;
                p = skip_whitespace(p);
                if (!parse_float(p, result.float_value)) {
                    result.error_msg = "Invalid voltage value";
                    return false;
                }
                result.has_float = true;
                result.valid = true;
            }
            return true;
        }

        if (strncasecmp_local(p, "CURR", 4) == 0) {
            p += 4;
            if (*p == '?') {
                result.type = ScpiCommandType::GET_CURRENT;
                result.is_query = true;
                result.valid = true;
            } else {
                result.type = ScpiCommandType::SET_CURRENT;
                p = skip_whitespace(p);
                if (!parse_float(p, result.float_value)) {
                    result.error_msg = "Invalid current value";
                    return false;
                }
                result.has_float = true;
                result.valid = true;
            }
            return true;
        }

        if (strncasecmp_local(p, "CODE", 4) == 0) {
            result.type = ScpiCommandType::SET_CODE;
            p += 4;
            p = skip_whitespace(p);
            if (!parse_int(p, result.int_value)) {
                result.error_msg = "Invalid code value";
                return false;
            }
            result.has_int = true;
            result.valid = true;
            return true;
        }

        if (strncasecmp_local(p, "PDOWN", 5) == 0) {
            result.type = ScpiCommandType::POWER_DOWN;
            result.valid = true;
            return true;
        }

        // CAL:GAIN, CAL:OFFS, CAL:EN - Calibration commands
        if (strncasecmp_local(p, "CAL:", 4) == 0) {
            p += 4;

            // CAL:GAIN
            if (strncasecmp_local(p, "GAIN", 4) == 0) {
                p += 4;
                if (*p == '?') {
                    result.type = ScpiCommandType::GET_CAL_GAIN;
                    result.is_query = true;
                    result.valid = true;
                } else {
                    result.type = ScpiCommandType::SET_CAL_GAIN;
                    p = skip_whitespace(p);
                    if (!parse_float(p, result.float_value)) {
                        result.error_msg = "Invalid gain value";
                        return false;
                    }
                    result.has_float = true;
                    result.valid = true;
                }
                return true;
            }

            // CAL:OFFS (offset)
            if (strncasecmp_local(p, "OFFS", 4) == 0) {
                p += 4;
                if (*p == '?') {
                    result.type = ScpiCommandType::GET_CAL_OFFSET;
                    result.is_query = true;
                    result.valid = true;
                } else {
                    result.type = ScpiCommandType::SET_CAL_OFFSET;
                    p = skip_whitespace(p);
                    if (!parse_float(p, result.float_value)) {
                        result.error_msg = "Invalid offset value";
                        return false;
                    }
                    result.has_float = true;
                    result.valid = true;
                }
                return true;
            }

            // CAL:EN (enable)
            if (strncasecmp_local(p, "EN", 2) == 0) {
                p += 2;
                if (*p == '?') {
                    result.type = ScpiCommandType::GET_CAL_ENABLE;
                    result.is_query = true;
                    result.valid = true;
                } else {
                    result.type = ScpiCommandType::SET_CAL_ENABLE;
                    p = skip_whitespace(p);
                    if (!parse_int(p, result.int_value)) {
                        result.error_msg = "Invalid enable value (0 or 1)";
                        return false;
                    }
                    result.has_int = true;
                    result.valid = true;
                }
                return true;
            }

            result.error_msg = "Unknown calibration command (use GAIN, OFFS, or EN)";
            return false;
        }

        result.error_msg = "Unknown channel command";
        return false;
    }

    if (strncasecmp_local(p, "SPAN", 4) == 0) {
        p += 4;

        // Check for :ALL
        if (*p == ':' && strncasecmp_local(p + 1, "ALL", 3) == 0) {
            result.type = ScpiCommandType::SET_ALL_SPAN;
            p += 4;
        } else {
            result.type = ScpiCommandType::SET_SPAN;
        }

        p = skip_whitespace(p);
        if (!parse_int(p, result.int_value)) {
            result.error_msg = "Invalid span value";
            return false;
        }
        result.has_int = true;
        result.valid = true;
        return true;
    }

    if (strncasecmp_local(p, "UPDATE", 6) == 0) {
        result.type = ScpiCommandType::UPDATE;
        result.valid = true;
        return true;
    }

    if (strncasecmp_local(p, "PDOWN", 5) == 0) {
        result.type = ScpiCommandType::POWER_DOWN_CHIP;
        result.valid = true;
        return true;
    }

    if (strncasecmp_local(p, "RES", 3) == 0) {
        p += 3;
        if (*p == '?') {
            result.type = ScpiCommandType::GET_RESOLUTION;
            result.is_query = true;
            result.valid = true;
        } else {
            result.type = ScpiCommandType::SET_RESOLUTION;
            p = skip_whitespace(p);
            if (!parse_int(p, result.int_value)) {
                result.error_msg = "Invalid resolution value (12 or 16)";
                return false;
            }
            if (result.int_value != 12 && result.int_value != 16) {
                result.error_msg = "Resolution must be 12 or 16";
                return false;
            }
            result.has_int = true;
            result.valid = true;
        }
        return true;
    }

    result.error_msg = "Unknown DAC command";
    return false;
}

ScpiCommand ScpiParser::parse(const std::string& line) {
    return parse(line.c_str());
}

ScpiCommand ScpiParser::parse(const char* line) {
    ScpiCommand result;

    // Skip leading whitespace
    line = skip_whitespace(line);

    if (*line == '\0') {
        result.error_msg = "Empty command";
        return result;
    }

    // Try different command types
    if (parse_common_command(line, result)) {
        return result;
    }

    if (parse_system_command(line, result)) {
        return result;
    }

    if (parse_board_command(line, result)) {
        return result;
    }

    result.error_msg = "Unknown command";
    return result;
}
