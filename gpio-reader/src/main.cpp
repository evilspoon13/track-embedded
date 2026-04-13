/**
 * main.cpp             GPIO Reader Process
 *
 * Interrupt-driven GPIO button reader. Owns all button GPIO lines and
 * signals target processes via SIGUSR1 when a button press is detected.
 *
 * @author      Cameron Stone '26 <cameron28202@gmail.com>
 *
 * @copyright   Texas A&M University
 */

#include <gpiod.h>

#include <cerrno>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>

static constexpr unsigned int GPIO_PIN_SCREEN_MOVE = 11;
static constexpr unsigned int GPIO_PIN_LOG_TOGGLE  = 13;

static constexpr int64_t DEBOUNCE_NS = 150'000'000; // 150ms

static constexpr const char* PIDFILE_GRAPHICS = "/run/track/graphics.pid";
static constexpr const char* PIDFILE_LOGGER   = "/run/track/logger.pid";

static volatile sig_atomic_t running = 1;

static void signal_handler(int) { running = 0; }

static pid_t read_pidfile(const char* path) {
    std::ifstream f(path);
    if (!f.is_open()) return 0;
    pid_t pid = 0;
    f >> pid;
    return pid;
}

static void signal_process(const char* pidfile, const char* name) {
    pid_t pid = read_pidfile(pidfile);
    if (pid <= 0) {
        printf("gpio: no pidfile for %s, skipping signal\n", name);
        return;
    }
    if (kill(pid, SIGUSR1) == 0) {
        printf("gpio: sent SIGUSR1 to %s (pid %d)\n", name, pid);
    } else {
        printf("gpio: failed to signal %s (pid %d): %s\n", name, pid, strerror(errno));
    }
}

int main() {
    struct sigaction sa{};
    sa.sa_handler = signal_handler;
    sigaction(SIGINT, &sa, nullptr);
    sigaction(SIGTERM, &sa, nullptr);

    gpiod_chip* chip = gpiod_chip_open("/dev/gpiochip0");
    if (!chip) {
        perror("gpio: failed to open /dev/gpiochip0");
        return 1;
    }

    gpiod_line_settings* settings = gpiod_line_settings_new();
    gpiod_line_settings_set_direction(settings, GPIOD_LINE_DIRECTION_INPUT);
    gpiod_line_settings_set_bias(settings, GPIOD_LINE_BIAS_PULL_UP);
    gpiod_line_settings_set_active_low(settings, true);
    gpiod_line_settings_set_edge_detection(settings, GPIOD_LINE_EDGE_RISING);
    gpiod_line_settings_set_debounce_period_us(settings, DEBOUNCE_NS / 1000);

    gpiod_line_config* config = gpiod_line_config_new();
    unsigned int offsets[] = { GPIO_PIN_SCREEN_MOVE, GPIO_PIN_LOG_TOGGLE };
    gpiod_line_config_add_line_settings(config, offsets, 2, settings);

    gpiod_line_request* request = gpiod_chip_request_lines(chip, nullptr, config);
    if (!request) {
        perror("gpio: failed to request lines");
        gpiod_line_config_free(config);
        gpiod_line_settings_free(settings);
        gpiod_chip_close(chip);
        return 1;
    }

    gpiod_line_config_free(config);
    gpiod_line_settings_free(settings);

    printf("GPIO reader started. Waiting for button events...\n");

    gpiod_edge_event_buffer* event_buf = gpiod_edge_event_buffer_new(8);

    while (running) {
        // block up to 500ms waiting for edge events, then re-check running flag
        int ret = gpiod_line_request_wait_edge_events(request, 500'000'000);
        if (ret < 0) {
            if (errno == EINTR) continue;
            perror("gpio: wait_edge_events failed");
            break;
        }
        if (ret == 0) continue; // timeout, loop back

        int num = gpiod_line_request_read_edge_events(request, event_buf, 8);
        if (num < 0) {
            if (errno == EINTR) continue;
            perror("gpio: read_edge_events failed");
            break;
        }

        for (int i = 0; i < num; ++i) {
            gpiod_edge_event* ev = gpiod_edge_event_buffer_get_event(event_buf, i);
            unsigned int pin = gpiod_edge_event_get_line_offset(ev);

            if (pin == GPIO_PIN_SCREEN_MOVE) {
                signal_process(PIDFILE_GRAPHICS, "graphics");
            } else if (pin == GPIO_PIN_LOG_TOGGLE) {
                signal_process(PIDFILE_LOGGER, "data-logger");
            }
        }
    }

    printf("GPIO reader shutting down\n");
    gpiod_edge_event_buffer_free(event_buf);
    gpiod_line_request_release(request);
    gpiod_chip_close(chip);
    return 0;
}
