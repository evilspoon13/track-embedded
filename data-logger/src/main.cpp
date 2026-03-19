/**
 * main.cpp             Data Logger Process
 *
 * @author      Cameron Stone '26 <cameron28202@gmail.com>
 *
 * @copyright   Texas A&M University
 */

#include <csignal>
#include <cstdio>
#include <unistd.h>

#include "shared_memory.hpp"
#include "log_writer.hpp"

static volatile sig_atomic_t running = 1;

static void signal_handler(int) {
    running = 0;
}

int main() {
    struct sigaction sa{};
    sa.sa_handler = signal_handler;
    sigaction(SIGINT, &sa, nullptr);
    sigaction(SIGTERM, &sa, nullptr);

    TelemetryQueue* queue = open_shared_queue(false);
    if (!queue) {
        std::perror("Failed to open shared memory queue");
        return 1;
    }

    LogWriter writer;
    if (!writer.is_open()) {
        close_shared_queue(queue, false);
        return 1;
    }

    std::size_t pos = queue->current_pos();
    printf("Data logger started. waiting for telemetry..\n");

    while (running) {
        std::size_t prev = pos;
        queue->consume(pos, [&](const TelemetryMessage& msg) {
            writer.write(msg.can_id, msg.value);
        });
        if (pos == prev) {
            usleep(1000); // 1ms sleep when idle
        } else {
            writer.flush();
        }
    }

    close_shared_queue(queue, false);
    return 0;
}
