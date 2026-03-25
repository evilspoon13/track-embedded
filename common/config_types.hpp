/**
 * config_types.hpp     Config Type Definitions
 *
 * @author      Cameron Stone '26 <cameron28202@gmail.com>
 * @author      Justin Busker '26
 *
 * @copyright   Texas A&M University
 */

#ifndef TRACK_CONFIG_TYPES_HPP
#define TRACK_CONFIG_TYPES_HPP

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>
#include <cstring>

// CAN frame config types
enum class SignalType {
    UINT8,
    INT8,
    UINT16,
    INT16,
    UINT32,
    INT32,
    FLOAT,
    DOUBLE
};

struct ChannelConfig {
    std::string name;
    uint8_t start_byte;
    uint8_t length;
    SignalType type;
    double scale;
    double offset;
};


// maps can ID to a list of channels that exist in that frame
using FrameMap = std::unordered_map<uint32_t, std::vector<ChannelConfig>>;

// Display config types — mirrors graphics.types.ts

enum class WidgetType { Gauge, Bar, Number, Indicator };

enum class DataUnit { Temperature, Pressure, RPM, Voltage, Current, Speed, Torque, Power, Percent };

struct PositionConfig {
    int x;
    int y;
    int width;
    int height;
};

struct DataConfig {
    uint32_t can_id;
    std::string can_id_label;
    std::string signal;
    DataUnit unit;
    double min;
    double max;
    double caution_threshold;
    double critical_threshold;
};

struct WidgetConfig {
    WidgetType type;
    bool alarm;
    PositionConfig position;
    DataConfig data;
};

struct ScreenConfig {
    std::string name;
    std::vector<WidgetConfig> widgets;
};

struct DisplayConfig {
    std::vector<ScreenConfig> screens;
};

#endif
