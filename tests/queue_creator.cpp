#include <csignal>
#include <cstdio>
#include <unistd.h>

#include "shared_memory.hpp"

static volatile sig_atomic_t running = 1;

static void signal_handler(int) {
    running = 0;
}

// Creates and holds open the shared memory queue so other processes can attach.
// Use this in place of can-reader when testing components that only need the
// queue to exist (e.g. cloud-bridge upload path).
int main() {
    struct sigaction sa{};
    sa.sa_handler = signal_handler;
    sigaction(SIGINT, &sa, nullptr);
    sigaction(SIGTERM, &sa, nullptr);

    TelemetryQueue* queue = open_shared_queue(true);
    if (!queue) {
        std::perror("Failed to create shared memory queue");
        return 1;
    }

    printf("Shared memory queue created. Holding open until signal...\n");

    while (running) {
        sleep(1);
    }

    close_shared_queue(queue, true);
    printf("Queue released.\n");
    return 0;
}
