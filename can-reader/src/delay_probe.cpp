/**
 * delay_probe.cpp      One-way delay probe
 *
 * @author      Cameron Stone '26 <cameron28202@gmail.com>
 *
 * @copyright   Texas A&M University
 */

#include "delay_probe.hpp"

#include <cstdio>
#include <ctime>

#include "can_socket.hpp"

namespace {
uint64_t now_us() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return uint64_t(ts.tv_sec) * 1000000ull + uint64_t(ts.tv_nsec) / 1000ull;
}

uint32_t be32_to_u32(const uint8_t* p) {
    return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16)
         | (uint32_t(p[2]) << 8)  |  uint32_t(p[3]);
}
}

DelayProbe::DelayProbe()
    : last_probe_(clock::now() - PROBE_PERIOD),
      last_good_delay_(clock::now()) {}

void DelayProbe::tick(CanSocket& sock) {
    auto now = clock::now();
    if (now - last_probe_ < PROBE_PERIOD) return;

    can_frame req{};
    req.can_id = REQ_ID;
    req.can_dlc = 0;
    T1_ = now_us();
    if (sock.write(req)) {
        probe_pending_ = true;
    }
    last_probe_ = now;
}

bool DelayProbe::handle_frame(const can_frame& frame) {
    if (frame.can_id != RSP_ID || !probe_pending_) {
        return false;
    }

    uint64_t T4 = now_us();
    uint32_t T2 = be32_to_u32(&frame.data[0]);
    uint32_t T3 = be32_to_u32(&frame.data[4]);
    uint32_t T1_32 = uint32_t(T1_);
    uint32_t rtt  = uint32_t(T4) - T1_32;
    uint32_t proc = T3 - T2;
    last_delay_us_ = (rtt - proc) / 2;

    std::printf("one_way_delay=%u us (T1=%u T2=%u T3=%u T4=%u)\n", last_delay_us_, T1_32, T2, T3, uint32_t(T4));

    probe_pending_ = false;
    if (last_delay_us_ <= THRESHOLD_US) {
        last_good_delay_ = clock::now();
    }
    return true;
}

bool DelayProbe::healthy() const {
    return (clock::now() - last_good_delay_) < STALE_TIMEOUT;
}
