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

#include "telemetry_queue.hpp"
#include "shared_memory.hpp"
#include "status_shm.hpp"
#include "log_writer.hpp"

static constexpr const char* PIDFILE = "/run/track/logger.pid";

static volatile sig_atomic_t running = 1;
static volatile sig_atomic_t toggle_flag = 0;

static void signal_handler(int) { running = 0; }
static void sigusr1_handler(int) { toggle_flag = 1; }

static void write_pidfile() {
    FILE* f = fopen(PIDFILE, "w");
    if (f) {
        fprintf(f, "%d\n", getpid());
        fclose(f);
    }
}

static void remove_pidfile() {
    unlink(PIDFILE);
}

int main() {
    struct sigaction sa{};
    sa.sa_handler = signal_handler;
    sigaction(SIGINT, &sa, nullptr);
    sigaction(SIGTERM, &sa, nullptr);

    struct sigaction sa_usr{};
    sa_usr.sa_handler = sigusr1_handler;
    sigaction(SIGUSR1, &sa_usr, nullptr);

    write_pidfile();

    TelemetryQueue* queue = open_shared_queue<TelemetryQueue>(TELEMETRY_SHM, false);
    if (!queue) {
        std::perror("Failed to open shared memory queue");
        remove_pidfile();
        return 1;
    }

    TrackStatus* status = open_status_shm();
    if (status)
        status->logger_recording.store(0, std::memory_order_relaxed);

    LogWriter writer;
    bool recording = false;

    std::size_t pos = queue->current_pos();
    printf("Data logger started. Press log button to begin recording.\n");

    while (running) {

        // from gpio reader SIGUSR1
        if (toggle_flag) {
            toggle_flag = 0;
            recording = !recording;
            if (recording) {
                writer.open();
                // only reflect "recording" if the file actually opened
                recording = writer.is_open();
                printf(recording ? "Recording started\n" : "Recording failed to start\n");
            }
            else {
                writer.close();
                printf("Recording stopped\n");
            }
            if (status) {
                status->logger_recording.store(recording ? 1 : 0, std::memory_order_relaxed);
            }
        }

        std::size_t prev = pos;
        if (recording) {
            queue->consume(pos, [&](const TelemetryMessage& msg) {
                writer.write(msg.can_id, msg.value);
            });
        } else {
            // drain queue to stay current even when not recording
            queue->consume(pos, [](const TelemetryMessage&) {});
        }

        if (pos == prev) {
            usleep(1000);
        } else if (recording) {
            writer.flush();
        }
    }

    if (recording) {
        writer.close();
    }
    if (status) {
        status->logger_recording.store(0, std::memory_order_relaxed);
        close_status_shm(status);
    }

    close_shared_queue(queue, TELEMETRY_SHM, false);
    remove_pidfile();
    return 0;
}
