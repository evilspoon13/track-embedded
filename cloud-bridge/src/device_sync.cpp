/**
 * device_sync.cpp      Device Identity & Config Sync (cloud → Pi)
 *
 * @author      Cameron Stone '26 <cameron28202@gmail.com>
 *
 * @copyright   Texas A&M University
 */

#include "device_sync.hpp"

#include <cstdio>
#include <cstdlib>
#include <fstream>

#include <nlohmann/json.hpp>
#include <ixwebsocket/IXHttpClient.h>

static std::string get_env(const char *name, const char *fallback) {
    const char *val = std::getenv(name);
    return val ? val : fallback;
}

DeviceSync::DeviceSync(const std::string& device_config_path, const std::string& api_url)
    : config_path_(device_config_path), api_url_(api_url) {
    load_identity();
}

void DeviceSync::load_identity() {
    std::ifstream file(config_path_);
    if (!file.is_open()) {
        printf("no device.json found, using env vars\n");
        device_id_ = get_env("CB_DEVICE_ID", "dev-001");
        device_secret_ = get_env("CB_DEVICE_SECRET", "");
        team_members_.clear();
        return;
    }
    try {
        auto j = nlohmann::json::parse(file);
        device_id_ = j.value("device_id", "dev-001");
        device_secret_ = j.value("device_secret", "");
        team_members_.clear();
        if (j.contains("team_members") && j["team_members"].is_array()) {
            for (const auto &m : j["team_members"]) {
                if (m.is_string()) team_members_.push_back(m.get<std::string>());
            }
        }
    } catch (const std::exception &e) {
        printf("failed to parse device.json: %s\n", e.what());
        device_id_ = get_env("CB_DEVICE_ID", "dev-001");
        device_secret_ = get_env("CB_DEVICE_SECRET", "");
        team_members_.clear();
    }
}

void DeviceSync::reload() {
    printf("reloading device.json\n");
    load_identity();
}

void DeviceSync::registerWithCloud() {
    if (team_members_.empty()) {
        printf("Device Sync: no team members, skipping registration\n");
        return;
    }

    std::string url = api_url_ + "/api/devices/register";

    nlohmann::json body;
    body["teamMembers"] = team_members_;

    ix::HttpClient client;
    auto args = client.createRequest();
    args->extraHeaders["X-Device-ID"] = device_id_;
    args->extraHeaders["X-Device-Secret"] = device_secret_;
    args->extraHeaders["Content-Type"] = "application/json";
    args->body = body.dump();

    auto response = client.post(url, args->body, args);
    if (response->statusCode == 200) {
        printf("Device Sync: registered with cloud\n");
    } else {
        printf("Device Sync: registration failed: %d %s\n", response->statusCode, response->body.c_str());
    }
}

bool DeviceSync::handleTeamMembersUpdate(const std::string& msg) {
    nlohmann::json j;
    try {
        j = nlohmann::json::parse(msg);
    } catch (...) {
        return false;
    }

    if (j.value("type", "") != "team_members_update") return false;
    if (!j.contains("team_members") || !j["team_members"].is_array()) return false;

    std::vector<std::string> members;
    for (const auto& m : j["team_members"]) {
        if (m.is_string()) members.push_back(m.get<std::string>());
    }

    // read existing device.json, update team_members, write back
    nlohmann::json config;
    std::ifstream in(config_path_);
    if (in.is_open()) {
        config = nlohmann::json::parse(in, nullptr, false);
        if (config.is_discarded()) config = nlohmann::json::object();
        in.close();
    }
    config["team_members"] = members;

    std::ofstream out(config_path_);
    if (!out.is_open()) {
        printf("Device Sync: failed to write %s\n", config_path_.c_str());
        return true;
    }
    out << config.dump(2);
    out.close();
    printf("Device Sync: updated team_members in %s\n", config_path_.c_str());

    team_members_ = std::move(members);
    return true;
}
