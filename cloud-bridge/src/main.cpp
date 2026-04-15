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
#include <fstream>
#include <iterator>
#include <string>
#include <unistd.h>
#include <unordered_map>

#include <nlohmann/json.hpp>

#include "config_receiver.hpp"
#include "device_sync.hpp"
#include "log_uploader.hpp"
#include "shared_memory.hpp"
#include "status_shm.hpp"
#include "telemetry_queue.hpp"
#include "time_util.hpp"
#include "ws_client.hpp"

static volatile sig_atomic_t running = 1;
static volatile sig_atomic_t reload_flag = 0;
static volatile sig_atomic_t graphics_changed = 0;
static volatile sig_atomic_t dbc_changed = 0;

static void signal_handler(int) { running = 0; }
static void sighup_handler(int) { reload_flag = 1; }
static void sigusr1_handler(int) { graphics_changed = 1; }
static void sigusr2_handler(int) { dbc_changed = 1; }

static std::string get_env(const char *name, const char *fallback) {
    const char *val = std::getenv(name);
    return val ? val : fallback;
}

static bool try_send_graphics_upload(WsClient& ws, const std::string& device_id, const char* graphics_path) {
    std::ifstream f(graphics_path);
    if (!f.is_open()) {
        std::printf("[graphics_upload] failed to read %s\n", graphics_path);
        return false;
    }

    nlohmann::json content = nlohmann::json::parse(f, nullptr, false);
    if (!content.is_object()) {
        std::printf("[graphics_upload] invalid JSON in %s\n", graphics_path);
        return false;
    }

    nlohmann::json j;
    j["type"] = "graphics_upload";
    j["device_id"] = device_id;
    j["payload"] = std::move(content);

    if (!ws.send(j.dump())) return false;
    return true;
}

static bool try_send_dbc_upload(WsClient& ws, const std::string& device_id, const char* dbc_path) {
    std::ifstream f(dbc_path, std::ios::binary);
    if (!f.is_open()) {
        std::printf("[dbc_upload] failed to read %s\n", dbc_path);
        return false;
    }
    std::string raw((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());

    nlohmann::json j;
    j["type"] = "dbc_upload";
    j["device_id"] = device_id;
    j["payload"] = std::move(raw);

    if (!ws.send(j.dump())) return false;
    return true;
}

int main() {
    struct sigaction sa{};
    sa.sa_handler = signal_handler;
    sigaction(SIGINT, &sa, nullptr);
    sigaction(SIGTERM, &sa, nullptr);

    struct sigaction sa_hup{};
    sa_hup.sa_handler = sighup_handler;
    sigaction(SIGHUP, &sa_hup, nullptr);

    struct sigaction sa_usr1{};
    sa_usr1.sa_handler = sigusr1_handler;
    sigaction(SIGUSR1, &sa_usr1, nullptr);

    struct sigaction sa_usr2{};
    sa_usr2.sa_handler = sigusr2_handler;
    sigaction(SIGUSR2, &sa_usr2, nullptr);

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

    TrackStatus *status = open_status_shm();
    if (status)
        status->cloud_ws_connected.store(0, std::memory_order_relaxed);

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
    if (graphics_changed) {
        if (ws.is_connected()) {
            if (try_send_graphics_upload(ws, device_sync.device_id(), "/opt/track/config/graphics.json")) {
                graphics_changed = 0;
            }
        }
    }
    if (dbc_changed) {
        if (ws.is_connected()) {
            if (try_send_dbc_upload(ws, device_sync.device_id(), "/opt/track/config/display.dbc")) {
                dbc_changed = 0;
            }
        }
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
    if (status) {
        status->cloud_ws_connected.store(0, std::memory_order_relaxed);
        close_status_shm(status);
    }
    close_shared_queue(queue, TELEMETRY_SHM, false);
    return 0;
}
