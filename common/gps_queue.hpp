/**
 * gps_queue.hpp    GPS Queue Typedef
 *
 * @author      Cameron Stone '26 <cameron28202@gmail.com>
 *
 * @copyright   Texas A&M University
 */

#ifndef TRACK_GPS_QUEUE_HPP
#define TRACK_GPS_QUEUE_HPP

#include "broadcast_queue.hpp"
#include <cstdint>

struct GpsMessage {
    double latitude;      // decimal degrees (negative = south)
    double longitude;     // decimal degrees (negative = west)
    double speed_kmh;     // speed over ground
    double heading;       // course over ground (0-360)
    int64_t timestamp_ms;
    bool has_fix;
};

using GpsQueue = BroadcastQueue<GpsMessage, 256>;
inline constexpr const char* GPS_SHM = "/track_gps";

#endif