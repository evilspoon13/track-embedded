#ifndef NMEA_PARSER_HPP
#define NMEA_PARSER_HPP

#include "gps_queue.hpp"

bool parse_nmea(const char* to_read, GpsMessage& gps_message);

#endif