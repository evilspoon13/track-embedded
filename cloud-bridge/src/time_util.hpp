#ifndef TRACK_TIME_UTIL_HPP
#define TRACK_TIME_UTIL_HPP

#include <chrono>
#include <cstdint>

inline int64_t now_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}

#endif
