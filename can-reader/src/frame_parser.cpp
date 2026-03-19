/**
 * frame_parser.cpp     CAN Frame Parser
 *
 * @author      Cameron Stone '26 <cameron28202@gmail.com>
 *
 * @copyright   Texas A&M University
 */

#include "frame_parser.hpp"


template<typename T>
static double extract(const can_frame& frame, const ChannelConfig& cfg) {
    T raw_value = 0;
    memcpy(&raw_value, &frame.data[cfg.start_byte], sizeof(T));
    return raw_value * cfg.scale + cfg.offset;
}

double parse_value(const can_frame& frame, const ChannelConfig& cfg) {
    switch (cfg.type) {
        case SignalType::UINT8:     return extract<uint8_t>(frame, cfg);
        case SignalType::INT8:      return extract<int8_t>(frame, cfg);
        case SignalType::UINT16:    return extract<uint16_t>(frame, cfg);
        case SignalType::INT16:     return extract<int16_t>(frame, cfg);
        case SignalType::UINT32:    return extract<uint32_t>(frame, cfg);
        case SignalType::INT32:     return extract<int32_t>(frame, cfg);
        case SignalType::FLOAT:     return extract<float>(frame, cfg);
        case SignalType::DOUBLE:    return extract<double>(frame, cfg);
    }
    return 0.0;
}
