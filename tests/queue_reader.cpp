/**
 * queue_reader.cpp     Queue Reader Test
 *
 * @author      Cameron Stone '26 <cameron28202@gmail.com>
 *
 * @copyright   Texas A&M University
 */

#include <csignal>
#include <cstdio>

#include "telemetry_queue.hpp"
#include "shared_memory.hpp"

static volatile sig_atomic_t running = 1;

static void signal_handler(int) {
    running = 0;
}

int main() {
    struct sigaction sa{};
    sa.sa_handler = signal_handler;
    sigaction(SIGINT, &sa, nullptr);
    sigaction(SIGTERM, &sa, nullptr);

    TelemetryQueue* queue = open_shared_queue<TelemetryQueue>(TELEMETRY_SHM, false);
    if (!queue) {
        std::perror("Failed to open shared memory queue");
        return 1;
    }

    std::size_t pos = queue->current_pos();
    printf("Attached to queue at pos %zu. Waiting for messages...\n", pos);

    while (running) {
        queue->consume(pos, [](const TelemetryMessage& msg) {
            printf("can_id=0x%03x  value=%f\n", msg.can_id, msg.value);
        });
    }

    close_shared_queue(queue, TELEMETRY_SHM, false);
    return 0;
}
