/**
 * main.cpp             CAN Reader Process
 *
 * @author      Cameron Stone '26 <cameron28202@gmail.com>
 *
 * @copyright   Texas A&M University
 */

#include <csignal>
#include <cstdio>
#include <cstring>

#include <chrono>

#include "telemetry_queue.hpp"
#include "dbc_parser.hpp"
#include "shared_memory.hpp"
#include "status_shm.hpp"
#include "can_socket.hpp"
#include "frame_parser.hpp"

static volatile sig_atomic_t running = 1;
static volatile sig_atomic_t reload_flag = 0;

static void signal_handler(int sig) {
    if (sig == SIGINT || sig == SIGTERM) running = 0;
    if (sig == SIGHUP) reload_flag = 1;
}

int main(int argc, char* argv[]) {
    const char* can_iface = (argc > 1) ? argv[1] : "can0";

    struct sigaction sa{};
    sa.sa_handler = signal_handler;
    sigaction(SIGINT, &sa, nullptr);
    sigaction(SIGTERM, &sa, nullptr);
    sigaction(SIGHUP, &sa, nullptr);

    FrameMap frame_map = load_dbc_config(DEFAULT_DBC_PATH);
    if(frame_map.empty()) {
        std::fprintf(stderr, "Failed to load CAN config\n");
        return 1;
    }

    TelemetryQueue* queue = open_shared_queue<TelemetryQueue>(TELEMETRY_SHM, true);
    if (!queue) {
        std::perror("Failed to open shared memory queue");
        return 1;
    }

    CanSocket sock;
    if( !sock.open(can_iface)) {
        std::perror("Failed to open CAN socket");
        close_shared_queue(queue, TELEMETRY_SHM, true);
        return 1;
    }

    // 100ms read timeout so the health check can age out
    sock.set_read_timeout(100);

    TrackStatus* status = open_status_shm();
    if (status)
        status->can_healthy.store(0, std::memory_order_relaxed);

    using clock = std::chrono::steady_clock;
    constexpr auto HEALTH_TIMEOUT = std::chrono::milliseconds(1000);
    auto last_frame = clock::now();
    int healthy_reported = -1;

    can_frame frame;
    while(running) {
        bool got_frame = sock.read(frame);
        auto now = clock::now();

        if (got_frame) last_frame = now;

        const int healthy_i = (now - last_frame) < HEALTH_TIMEOUT ? 1 : 0;
        if (status && healthy_i != healthy_reported) {
            status->can_healthy.store(uint8_t(healthy_i), std::memory_order_relaxed);
            healthy_reported = healthy_i;
        }

        if (got_frame) {
            printf("Received CAN frame with ID: %03x\n", frame.can_id);
            auto it = frame_map.find(frame.can_id);
            if (it == frame_map.end()) {
                continue;
            }

            const auto& channels = it->second;

            for (const auto& cfg : channels) {
                TelemetryMessage msg;
                msg.can_id = frame.can_id;
                std::strncpy(msg.signal_name, cfg.name.c_str(), sizeof(msg.signal_name) - 1);
                msg.signal_name[sizeof(msg.signal_name) - 1] = '\0';
                msg.value = parse_value(frame, cfg);
                queue->push(msg);
                printf("Parsed signal '%s' for CAN ID %03x: %f\n", msg.signal_name, frame.can_id, msg.value);
            }
        }
        if (reload_flag) {
            reload_flag = 0;
            frame_map = load_dbc_config(DEFAULT_DBC_PATH);
            printf("Reloaded config\n");
        }
    }

    if (status) {
        status->can_healthy.store(0, std::memory_order_relaxed);
        close_status_shm(status);
    }
    close_shared_queue(queue, TELEMETRY_SHM, true);

    return 0;
}
