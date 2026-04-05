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
#include <string>
#include <unistd.h>
#include <unordered_map>
#include <vector>

#include <nlohmann/json.hpp>
#include <ixwebsocket/IXHttpClient.h>

#include "config_receiver.hpp"
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

static constexpr const char *DEVICE_CONFIG_PATH = "/opt/track/config/device.json";

struct DeviceIdentity {
  std::string device_id;
  std::string device_secret;
  std::vector<std::string> team_members;
};

static DeviceIdentity read_device_config() {
  DeviceIdentity id;
  std::ifstream file(DEVICE_CONFIG_PATH);
  if (!file.is_open()) {
    printf("no device.json found, using env vars\n");
    id.device_id = get_env("CB_DEVICE_ID", "dev-001");
    id.device_secret = get_env("CB_DEVICE_SECRET", "");
    return id;
  }
  try {
    auto j = nlohmann::json::parse(file);
    id.device_id = j.value("device_id", "dev-001");
    id.device_secret = j.value("device_secret", "");
    if (j.contains("team_members") && j["team_members"].is_array()) {
      for (const auto &m : j["team_members"]) {
        if (m.is_string()) id.team_members.push_back(m.get<std::string>());
      }
    }
  } catch (const std::exception &e) {
    printf("failed to parse device.json: %s\n", e.what());
    id.device_id = get_env("CB_DEVICE_ID", "dev-001");
    id.device_secret = get_env("CB_DEVICE_SECRET", "");
  }
  return id;
}

static void register_with_cloud(const std::string &api_url, const DeviceIdentity &id) {
  if (id.team_members.empty()) {
    printf("[register] no team members, skipping registration\n");
    return;
  }

  std::string url = api_url + "/api/devices/register";

  nlohmann::json body;
  body["teamMembers"] = id.team_members;

  ix::HttpClient client;
  auto args = client.createRequest();
  args->extraHeaders["X-Device-ID"] = id.device_id;
  args->extraHeaders["X-Device-Secret"] = id.device_secret;
  args->extraHeaders["Content-Type"] = "application/json";
  args->body = body.dump();

  auto response = client.post(url, args->body, args);
  if (response->statusCode == 200) {
    printf("device registered successfully\n");
  } else {
    printf("registration failed: %d %s\n", response->statusCode, response->body.c_str());
  }
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
  DeviceIdentity device = read_device_config();

  constexpr int send_interval_ms = 50;
  constexpr int heartbeat_interval_ms = 1000;

  // reader of queue
  TelemetryQueue *queue = open_shared_queue<TelemetryQueue>(TELEMETRY_SHM, false);
  if (!queue) {
    std::perror("Failed to open shared memory queue");
    return 1;
  }

  WsClient ws(ws_url, device.device_id, device.device_secret);

  ConfigReceiver config_receiver("/tmp/graphics.json");

  LogUploader log_uploader("/tmp/track-logs", ws, device.device_id);

  log_uploader.start();

  ws.set_on_message([&config_receiver](const std::string &msg) {
    printf("[config] received: %s\n", msg.c_str());
    config_receiver.ReceiveCallback(msg);
  });

  ws.start();

  register_with_cloud(api_url, device);

  std::size_t pos = queue->current_pos();
  std::unordered_map<std::string, double> signals;
  int64_t last_send_time = 0;
  int64_t last_heartbeat_time = 0;

  printf("Cloud bridge started. ws=%s api=%s device=%s\n", ws_url.c_str(), api_url.c_str(), device.device_id.c_str());

  while (running) {
    // SIGHUP: re-read device.json and re-register (triggered by captive portal)
    if (reload_flag) {
        reload_flag = 0;
        printf("reloading device.json\n");
        device = read_device_config();
        register_with_cloud(api_url, device);
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
        j["device_id"] = device.device_id;
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
      heartbeat["device_id"] = device.device_id;
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
