/**
 * main.cpp             GPIO Reader Process
 *
 * Interrupt-driven GPIO button reader + LED driver. Owns all button input
 * lines and the 3 status LED output lines. Signals target processes via
 * SIGUSR1 on button press. Polls /track_status shm at 10 Hz to drive LEDs.
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
#include <sys/wait.h>
#include <unistd.h>

#include "status_shm.hpp"

// buttons (BCM line numbers — physical pins 11, 13, 10)
static constexpr unsigned int GPIO_PIN_SCREEN_MOVE = 17;
static constexpr unsigned int GPIO_PIN_LOG_TOGGLE  = 27;
static constexpr unsigned int GPIO_PIN_WIFI_TOGGLE = 15;

// leds (BCM line numbers — physical pins 16, 18, 29)
static constexpr unsigned int GPIO_LED_GREEN = 23; // data logger recording
static constexpr unsigned int GPIO_LED_BLUE  = 24; // cloud ws connected
static constexpr unsigned int GPIO_LED_RED   = 5;  // can bus healthy -- red on fault

static constexpr int64_t DEBOUNCE_NS      = 150'000'000; // 150ms
static constexpr int64_t POLL_INTERVAL_NS = 100'000'000; // 100ms LED refresh

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

static void toggle_wifi_mode() {
    pid_t pid = fork();
    if (pid < 0) {
        perror("gpio: fork failed");
        return;
    }
    if (pid == 0) {
        execl("/bin/sh", "sh", "-c",
              "systemctl is-active --quiet hostapd "
              "&& /opt/track/scripts/wifi-mode.sh "
              "|| /opt/track/scripts/ap-mode.sh",
              nullptr);
        perror("gpio: execl failed");
        _exit(127);
    }
    printf("gpio: launched wifi/ap toggle (pid %d)\n", pid);
}

static void set_led(gpiod_line_request* req, unsigned int pin, bool on) {
    gpiod_line_request_set_value(req, pin, on ? GPIOD_LINE_VALUE_ACTIVE : GPIOD_LINE_VALUE_INACTIVE);
}

int main() {
    struct sigaction sa{};
    sa.sa_handler = signal_handler;
    sigaction(SIGINT, &sa, nullptr);
    sigaction(SIGTERM, &sa, nullptr);

    // auto-reap children so toggle_wifi_mode() doesn't leave zombies
    struct sigaction chld{};
    chld.sa_handler = SIG_IGN;
    sigaction(SIGCHLD, &chld, nullptr);

    gpiod_chip* chip = gpiod_chip_open("/dev/gpiochip0");
    if (!chip) {
        perror("gpio: failed to open /dev/gpiochip0");
        return 1;
    }

    // input line settings (buttons)
    gpiod_line_settings* in_settings = gpiod_line_settings_new();
    gpiod_line_settings_set_direction(in_settings, GPIOD_LINE_DIRECTION_INPUT);
    gpiod_line_settings_set_bias(in_settings, GPIOD_LINE_BIAS_PULL_UP);
    gpiod_line_settings_set_active_low(in_settings, true);
    gpiod_line_settings_set_edge_detection(in_settings, GPIOD_LINE_EDGE_RISING);
    gpiod_line_settings_set_debounce_period_us(in_settings, DEBOUNCE_NS / 1000);

    // output line settings (LEDs)
    gpiod_line_settings* out_settings = gpiod_line_settings_new();
    gpiod_line_settings_set_direction(out_settings, GPIOD_LINE_DIRECTION_OUTPUT);
    gpiod_line_settings_set_output_value(out_settings, GPIOD_LINE_VALUE_INACTIVE);

    gpiod_line_config* config = gpiod_line_config_new();
    unsigned int in_offsets[]  = { GPIO_PIN_SCREEN_MOVE, GPIO_PIN_LOG_TOGGLE, GPIO_PIN_WIFI_TOGGLE };
    unsigned int out_offsets[] = { GPIO_LED_GREEN, GPIO_LED_BLUE, GPIO_LED_RED };
    gpiod_line_config_add_line_settings(config, in_offsets, 3, in_settings);
    gpiod_line_config_add_line_settings(config, out_offsets, 3, out_settings);

    gpiod_line_request* request = gpiod_chip_request_lines(chip, nullptr, config);
    if (!request) {
        perror("gpio: failed to request lines");
        gpiod_line_config_free(config);
        gpiod_line_settings_free(in_settings);
        gpiod_line_settings_free(out_settings);
        gpiod_chip_close(chip);
        return 1;
    }

    gpiod_line_config_free(config);
    gpiod_line_settings_free(in_settings);
    gpiod_line_settings_free(out_settings);

    TrackStatus* status = open_status_shm();
    if (!status) {
        fprintf(stderr, "gpio: failed to open status shm (LEDs will stay off)\n");
    }

    int last_green = -1, last_blue = -1, last_red = -1;

    printf("GPIO reader started. waiting for button events + driving LEDs\n");

    gpiod_edge_event_buffer* event_buf = gpiod_edge_event_buffer_new(8);

    while (running) {
        // Wake up at least every POLL_INTERVAL_NS to refresh LEDs, sooner if an edge event arrives
        int ret = gpiod_line_request_wait_edge_events(request, POLL_INTERVAL_NS);
        if (ret < 0) {
            if (errno == EINTR) continue;
            perror("gpio: wait_edge_events failed");
            break;
        }

        if (ret > 0) {
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
                }
                else if (pin == GPIO_PIN_LOG_TOGGLE) {
                    signal_process(PIDFILE_LOGGER, "data-logger");
                }
                else if (pin == GPIO_PIN_WIFI_TOGGLE) {
                    toggle_wifi_mode();
                }
            }
        }

        if (status) {
            int green = status->logger_recording.load(std::memory_order_relaxed) ? 1 : 0;
            int blue = status->cloud_ws_connected.load(std::memory_order_relaxed) ? 1 : 0;
            int red = status->can_healthy.load(std::memory_order_relaxed) ? 0 : 1;
            if (green != last_green) {
                set_led(request, GPIO_LED_GREEN, green);
                last_green = green;
            }
            if (blue != last_blue) {
                set_led(request, GPIO_LED_BLUE,  blue);
                last_blue = blue;
            }
            if (red != last_red) {
                set_led(request, GPIO_LED_RED,   red);
                last_red = red;
            }
        }
    }

    printf("GPIO reader shutting down\n");
    set_led(request, GPIO_LED_GREEN, false);
    set_led(request, GPIO_LED_BLUE,  false);
    set_led(request, GPIO_LED_RED,   false);
    close_status_shm(status);
    gpiod_edge_event_buffer_free(event_buf);
    gpiod_line_request_release(request);
    gpiod_chip_close(chip);
    return 0;
}
