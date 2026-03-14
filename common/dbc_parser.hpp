#ifndef TRACK_DBC_PARSER_HPP
#define TRACK_DBC_PARSER_HPP

#include "config_types.hpp"

constexpr const char* DEFAULT_DBC_PATH = "/tmp/display.dbc";

FrameMap load_dbc_config(const std::string& path);

#endif
