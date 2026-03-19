/**
 * shared_memory.hpp    Shared Memory IPC
 *
 * @author      Cameron Stone '26 <cameron28202@gmail.com>
 *
 * @copyright   Texas A&M University
 */

#ifndef TRACK_SHARED_MEMORY_HPP
#define TRACK_SHARED_MEMORY_HPP

#include <cstddef>

#include "broadcast_queue.hpp"
#include "config_types.hpp"

inline constexpr const char* SHM_NAME = "/track_telemetry";

using TelemetryQueue = BroadcastQueue<TelemetryMessage, 4096>;

// open queue, return pointer to shared mem
TelemetryQueue* open_shared_queue(bool is_writer);

// unmap and close shared mem queue
void close_shared_queue(TelemetryQueue* queue, bool is_writer);

#endif
