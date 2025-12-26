#include "utils.hpp"
#include <stdexcept>

namespace utils {

std::vector<std::string> split(const std::string &s, char delim) {
    std::vector<std::string> tokens;
    std::stringstream ss(s);
    std::string token;

    while (std::getline(ss, token, delim)) {
        // Skip empty tokens
        if (!token.empty()) {
            tokens.push_back(token);
        }
    }

    return tokens;
}

uint16_t parse_hex(const std::string &s) {
    // Handle optional "0x" or "0X" prefix
    size_t start = 0;
    if (s.size() >= 2 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
        start = 2;
    }

    uint16_t result = 0;
    for (size_t i = start; i < s.size(); ++i) {
        char c = s[i];
        uint8_t digit;

        if (c >= '0' && c <= '9') {
            digit = c - '0';
        } else if (c >= 'a' && c <= 'f') {
            digit = 10 + (c - 'a');
        } else if (c >= 'A' && c <= 'F') {
            digit = 10 + (c - 'A');
        } else {
            // Invalid character - return what we have so far
            break;
        }

        result = (result << 4) | digit;
    }

    return result;
}

bool parse_int(const std::string &s, int32_t &out) {
    if (s.empty()) {
        return false;
    }

    size_t i = 0;
    bool negative = false;

    // Handle sign
    if (s[0] == '-') {
        negative = true;
        i = 1;
    } else if (s[0] == '+') {
        i = 1;
    }

    if (i >= s.size()) {
        return false;
    }

    int32_t result = 0;
    for (; i < s.size(); ++i) {
        char c = s[i];
        if (c < '0' || c > '9') {
            return false;
        }
        result = result * 10 + (c - '0');
    }

    out = negative ? -result : result;
    return true;
}

bool parse_float(const std::string &s, float &out) {
    if (s.empty()) {
        return false;
    }

    size_t i = 0;
    bool negative = false;

    // Handle sign
    if (s[0] == '-') {
        negative = true;
        i = 1;
    } else if (s[0] == '+') {
        i = 1;
    }

    if (i >= s.size()) {
        return false;
    }

    float result = 0.0f;
    bool has_decimal = false;
    float decimal_place = 0.1f;

    for (; i < s.size(); ++i) {
        char c = s[i];

        if (c == '.') {
            if (has_decimal) {
                return false;  // Multiple decimal points
            }
            has_decimal = true;
            continue;
        }

        if (c < '0' || c > '9') {
            return false;
        }

        if (has_decimal) {
            result += (c - '0') * decimal_place;
            decimal_place *= 0.1f;
        } else {
            result = result * 10.0f + (c - '0');
        }
    }

    out = negative ? -result : result;
    return true;
}

} // namespace utils
