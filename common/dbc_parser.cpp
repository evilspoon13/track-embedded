/**
 * dbc_parser.cpp       DBC File Parser
 *
 * @author      Cameron Stone '26 <cameron28202@gmail.com>
 *
 * @copyright   Texas A&M University
 */

#include <fstream>
#include <string>
#include <regex>

#include "dbc_parser.hpp"

// derive SignalType from bit length and signedness
static SignalType derive_signal_type(int bit_length, bool is_signed) {
    if (bit_length <= 8)  return is_signed ? SignalType::INT8  : SignalType::UINT8;
    if (bit_length <= 16) return is_signed ? SignalType::INT16 : SignalType::UINT16;
    if (bit_length <= 32) return is_signed ? SignalType::INT32 : SignalType::UINT32;
    return SignalType::DOUBLE;
}

FrameMap load_dbc_config(const std::string& path) {
    FrameMap result;
    std::ifstream file(path);
    if (!file.is_open()) return result;

    // BO_ <id> <name>: <dlc> <sender>
    std::regex bo_re(R"(^BO_\s+(\d+)\s+(\w+)\s*:\s*(\d+)\s+(\w+))");

    // SG_ <name> : <start_bit>|<bit_length>@<byte_order><sign> (<scale>,<offset>) [<min>|<max>] "<unit>" <receivers>
    std::regex sg_re(R"(^\s+SG_\s+(\w+)\s*:\s*(\d+)\|(\d+)@([01])([+-])\s*\(([^,]+),([^)]+)\))");

    std::string line;
    uint32_t current_id = 0;
    bool in_message = false;

    while (std::getline(file, line)) {
        std::smatch match;

        if (std::regex_search(line, match, bo_re)) {
            current_id = static_cast<uint32_t>(std::stoul(match[1].str()));
            in_message = true;
            continue;
        }

        if (in_message && std::regex_search(line, match, sg_re)) {
            std::string name = match[1].str();
            int start_bit    = std::stoi(match[2].str());
            int bit_length   = std::stoi(match[3].str());
            // match[4] = byte order: 1 = little-endian, 0 = big-endian
            char sign        = match[5].str()[0];
            double scale     = std::stod(match[6].str());
            double offset    = std::stod(match[7].str());

            ChannelConfig cfg;
            cfg.name       = name;
            cfg.start_byte = static_cast<uint8_t>(start_bit / 8);
            cfg.length     = static_cast<uint8_t>(bit_length / 8);
            cfg.type       = derive_signal_type(bit_length, sign == '-');
            cfg.scale      = scale;
            cfg.offset     = offset;

            result[current_id].emplace_back(cfg);
            continue;
        }

        // a non-indented, non-empty line that isn't a signal ends the current message
        if (in_message && !line.empty() && line[0] != ' ' && line[0] != '\t') {
            in_message = false;
        }
    }

    return result;
}
