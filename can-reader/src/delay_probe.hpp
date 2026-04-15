/**
 * delay_probe.hpp      One-way delay probe (PTP style 4-timestamp)
 *
 * TRACK sends 0x7E0 (empty), sim node replies 0x7E1 with T2||T3 as
 * big-endian u32 microseconds. One-way delay estimate:
 *
 *     one_way_delay = ((T4 - T1) - (T3 - T2)) / 2
 *
 * Drives CAN health: healthy when a measurement <= threshold has been
 * observed within the stale timeout.
 *
 * @author      Cameron Stone '26 <cameron28202@gmail.com>
 *
 * @copyright   Texas A&M University
 */

#ifndef TRACK_DELAY_PROBE_HPP
#define TRACK_DELAY_PROBE_HPP

#include <chrono>
#include <cstdint>
#include <linux/can.h>

class CanSocket;

class DelayProbe {
public:
    static constexpr canid_t REQ_ID = 0x7E0;
    static constexpr canid_t RSP_ID = 0x7E1;
    static constexpr auto PROBE_PERIOD = std::chrono::seconds(1);
    static constexpr uint32_t THRESHOLD_US = 5000;
    static constexpr auto STALE_TIMEOUT = std::chrono::milliseconds(3000);

    DelayProbe();

    // Send a probe frame if it's time; stamp T1.
    void tick(CanSocket& sock);

    // If frame is a reply, consume it and return true.
    bool handle_frame(const can_frame& frame);

    bool healthy() const;

    uint32_t last_delay_us() const { return last_delay_us_; }

private:
    using clock = std::chrono::steady_clock;

    clock::time_point last_probe_;
    clock::time_point last_good_delay_;
    uint64_t T1_ = 0;
    uint32_t last_delay_us_ = 0;
    bool probe_pending_ = false;
};

#endif
