/**
 * main.cpp             CAN Reader Process
 *
 * @author      Cameron Stone '26 <cameron28202@gmail.com>
 *
 * @copyright   Texas A&M University
 */

#include <csignal>
#include <cstdio>

#include "telemetry_queue.hpp"
#include "dbc_parser.hpp"
#include "shared_memory.hpp"
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
    sa.sa_handler = signal_handler;
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

    can_frame frame;
    while(running) {
        if (sock.read(frame)) {

            printf("Received CAN frame with ID: %03x\n", frame.can_id);
            auto it = frame_map.find(frame.can_id);
            if (it == frame_map.end()) {
                continue;
            }

            const auto& channels = it->second;

            for (const auto& cfg : channels) {
                // build telemetry message
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

    close_shared_queue(queue, TELEMETRY_SHM, true);

    return 0;
}
