#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <unordered_map>
#include <unistd.h>

#include <nlohmann/json.hpp>

#include "config_receiver.hpp"
#include "log_uploader.hpp"
#include "shared_memory.hpp"
#include "ws_client.hpp"

static volatile sig_atomic_t running = 1;

static void signal_handler(int) {
    running = 0;
}

static std::string get_env(const char* name, const char* fallback) {
    const char* val = std::getenv(name);
    return val ? val : fallback;
}

static int64_t now_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

int main() {
    struct sigaction sa{};
    sa.sa_handler = signal_handler;
    sigaction(SIGINT, &sa, nullptr);
    sigaction(SIGTERM, &sa, nullptr);

    // pull from env, fall back to defaults if not
    std::string url = get_env("CB_URL", "ws://localhost:9002");
    std::string device_id = get_env("CB_DEVICE_ID", "dev-001");
    std::string device_secret = get_env("CB_DEVICE_SECRET", "");

    // 20hz .. don't need crazy high update rates for telem
    constexpr int send_interval_ms = 50;
    constexpr int heartbeat_interval_ms = 1000;

    // reader of queue
    TelemetryQueue* queue = open_shared_queue(false);
    if (!queue) {
        std::perror("Failed to open shared memory queue");
        return 1;
    }

    WsClient ws(url, device_id, device_secret);

    ConfigReceiver config_receiver("/tmp/graphics.json");

    LogUploader log_uploader("/tmp/track-logs");

    log_uploader.start();

    ws.set_on_message([&config_receiver](const std::string& msg) {
        printf("[config] received: %s\n", msg.c_str());
        config_receiver.ReceiveCallback(msg);
    });

    ws.start();

    std::size_t pos = queue->current_pos();
    std::unordered_map<std::string, double> signals;
    int64_t last_send_time = 0;
    int64_t last_heartbeat_time = 0;

    printf("Cloud bridge started. url=%s device=%s\n", url.c_str(), device_id.c_str());

    while (running) {
        std::size_t prev = pos;
        queue->consume(pos, [&](const TelemetryMessage& msg) {
            std::string key = std::to_string(msg.can_id) + ":" + msg.signal_name;
            signals[key] = msg.value;
        });

        int64_t now = now_ms();
        if (now - last_send_time >= send_interval_ms) {
            if (!signals.empty()){
                nlohmann::json j;
                j["type"] = "telemetry";
                j["device_id"] = device_id;
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
            heartbeat["device_id"] = device_id;
            heartbeat["ts"] = now;
            ws.send(heartbeat.dump());
            last_heartbeat_time = now;
        }

        if (pos == prev) {
            usleep(1000);
        }
    }

    printf("Cloud bridge shutting down\n");
    ws.stop();
    log_uploader.stop();
    close_shared_queue(queue, false);
    return 0;
}
