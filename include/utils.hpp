#ifndef UTILS_HPP
#define UTILS_HPP

#include <vector>
#include <string>
#include <sstream>
#include <cstdint>

// helper funcs like string split, to_uint, etc
namespace utils {
    std::vector<std::string> split(const std::string &s, char delim);
    uint16_t parse_hex(const std::string &s);
    bool parse_int(const std::string &s, int32_t &out);
    bool parse_float(const std::string &s, float &out);
}

#endif // UTILS_HPP
