/**
 * main.cpp             GPS Reader Process
 *
 * @author      Cameron Stone '26 <cameron28202@gmail.com>
 *
 * @copyright   Texas A&M University
 */

#include <csignal>
#include <cstdio>
#include <cstdlib>

#include "serial_port.hpp"
#include "nmea_parser.hpp"
#include "shared_memory.hpp"

static volatile sig_atomic_t running = 1;
static volatile sig_atomic_t reload_flag = 0;

static void signal_handler(int sig) {
    if (sig == SIGINT || sig == SIGTERM) running = 0;
    if (sig == SIGHUP) reload_flag = 1;
}

int main() {
    const char* device = std::getenv("GPS_DEVICE");
    if (!device) device = "/dev/ttyAMA0";

    const char* baud_str = std::getenv("GPS_BAUD");
    int baud = baud_str ? std::atoi(baud_str) : 9600;

    struct sigaction sa{};
    sa.sa_handler = signal_handler;
    sigaction(SIGINT, &sa, nullptr);
    sigaction(SIGTERM, &sa, nullptr);
    sigaction(SIGHUP, &sa, nullptr);

    GpsQueue* queue = open_shared_queue<GpsQueue>(GPS_SHM, true);
    if (!queue) {
        std::perror("Failed to open GPS shared memory queue");
        return 1;
    }

    SerialPort sp;
    if (!sp.open(device, baud)) {
        std::fprintf(stderr, "Failed to open GPS serial port: %s @ %d\n", device, baud);
        close_shared_queue(queue, GPS_SHM, true);
        return 1;
    }

    std::printf("GPS reader started: %s @ %d baud\n", device, baud);

    GpsMessage current{};
    char line[256];

    while (running) {
        int n = sp.read_line(line, sizeof(line));
        if (n <= 0) continue;

        std::printf("RAW[%d]: %s", n, line);
        if (line[n > 0 ? n - 1 : 0] != '\n') std::printf("\n");
        std::fflush(stdout);

        if (parse_nmea(line, current)) {
            queue->push(current);
            std::printf("GPS: fix=%d lat=%.6f lon=%.6f speed=%.2f kmh heading=%.1f ts=%lld\n", current.has_fix, current.latitude, current.longitude, current.speed_kmh, current.heading, (long long)current.timestamp_ms);
            std::fflush(stdout);
        }

        if (reload_flag) {
            reload_flag = 0;
            std::printf("Config reload requested\n");
        }
    }

    sp.close();
    close_shared_queue(queue, GPS_SHM, true);

    std::printf("GPS reader stopped\n");
    return 0;
}
