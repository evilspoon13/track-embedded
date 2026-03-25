/**
 * telemetry_queue.hpp    Telemetry Queue Typedef
 *
 * @author      Cameron Stone '26 <cameron28202@gmail.com>
 *
 * @copyright   Texas A&M University
 */

#ifndef TRACK_TELEMETRY_QUEUE_HPP
#define TRACK_TELEMETRY_QUEUE_HPP

#include "broadcast_queue.hpp"
#include <cstdint>
#include <cstring>

struct TelemetryMessage {
    uint32_t can_id;
    char signal_name[64];
    double value;
};
using TelemetryQueue = BroadcastQueue<TelemetryMessage, 4096>;
inline constexpr const char* TELEMETRY_SHM = "/track_telemetry";

#endif