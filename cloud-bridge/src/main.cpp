/**
 * main.cpp             Cloud Bridge Process
 *
 * @author      Cameron Stone '26 <cameron28202@gmail.com>
 * @author      Brayden Bailey '26
 *
 * @copyright   Texas A&M University
 */

#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <unistd.h>
#include <unordered_map>

#include <nlohmann/json.hpp>

#include "config_receiver.hpp"
#include "device_sync.hpp"
#include "log_uploader.hpp"
#include "shared_memory.hpp"
#include "telemetry_queue.hpp"
#include "time_util.hpp"
#include "ws_client.hpp"

static volatile sig_atomic_t running = 1;
static volatile sig_atomic_t reload_flag = 0;

static void signal_handler(int) { running = 0; }
static void sighup_handler(int) { reload_flag = 1; }

static std::string get_env(const char *name, const char *fallback) {
    const char *val = std::getenv(name);
    return val ? val : fallback;
}

int main() {
    struct sigaction sa{};
    sa.sa_handler = signal_handler;
    sigaction(SIGINT, &sa, nullptr);
    sigaction(SIGTERM, &sa, nullptr);

    struct sigaction sa_hup{};
    sa_hup.sa_handler = sighup_handler;
    sigaction(SIGHUP, &sa_hup, nullptr);

    std::string ws_url = get_env("CB_URL", "wss://track-web.fly.dev/ws/pi");
    std::string api_url = get_env("CB_API_URL", "https://track-web.fly.dev");

    constexpr int send_interval_ms = 50;
    constexpr int heartbeat_interval_ms = 1000;

    // reader of queue
    TelemetryQueue *queue = open_shared_queue<TelemetryQueue>(TELEMETRY_SHM, false);
    if (!queue) {
    std::perror("Failed to open shared memory queue");
    return 1;
    }

    DeviceSync device_sync("/opt/track/config/device.json", api_url);

    WsClient ws(ws_url, device_sync.device_id(), device_sync.device_secret());

    ConfigReceiver config_receiver("/opt/track/config/graphics.json");

    LogUploader log_uploader("/tmp/track-logs", ws, device_sync.device_id());

    log_uploader.start();

    ws.set_on_message([&config_receiver, &device_sync](const std::string &msg) {
    printf("[ws] received: %s\n", msg.c_str());

    if (device_sync.handleTeamMembersUpdate(msg)) return;

    config_receiver.ReceiveCallback(msg);
    });

    ws.start();

    device_sync.registerWithCloud();

    std::size_t pos = queue->current_pos();
    std::unordered_map<std::string, double> signals;
    int64_t last_send_time = 0;
    int64_t last_heartbeat_time = 0;

    printf("Cloud bridge started. ws=%s api=%s device=%s\n", ws_url.c_str(), api_url.c_str(), device_sync.device_id().c_str());

    while (running) {
    if (reload_flag) {
        reload_flag = 0;
        device_sync.reload();
        device_sync.registerWithCloud();
    }

    std::size_t prev = pos;
    queue->consume(pos, [&](const TelemetryMessage &msg) {
        std::string key = std::to_string(msg.can_id) + ":" + msg.signal_name;
        signals[key] = msg.value;
    });

    int64_t now = now_ms();
    if (now - last_send_time >= send_interval_ms) {
        if (!signals.empty()) {
        nlohmann::json j;
        j["type"] = "telemetry";
        j["device_id"] = device_sync.device_id();
        j["ts"] = now;
        j["signals"] = signals;

        if (ws.send(j.dump())) {
            signals.clear();
        }
        }
        last_send_time = now;
    }

    if (now - last_heartbeat_time >= heartbeat_interval_ms) {
        nlohmann::json heartbeat;
        heartbeat["type"] = "heartbeat";
        heartbeat["status"] = "connected";
        heartbeat["device_id"] = device_sync.device_id();
        heartbeat["ts"] = now;
        ws.send(heartbeat.dump());
        last_heartbeat_time = now;
    }

    if (pos == prev) {
        usleep(1000);
    }
    }

    printf("Cloud bridge shutting down\n");
    log_uploader.stop();
    ws.stop();
    close_shared_queue(queue, TELEMETRY_SHM, false);
    return 0;
}
