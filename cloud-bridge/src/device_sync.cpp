/**
 * device_sync.cpp      Device Config Sync (cloud → Pi)
 *
 * @author      Cameron Stone '26 <cameron28202@gmail.com>
 *
 * @copyright   Texas A&M University
 */

#include "device_sync.hpp"

#include <cstdio>
#include <fstream>

#include <nlohmann/json.hpp>
#include <ixwebsocket/IXHttpClient.h>

DeviceSync::DeviceSync(const std::string& device_config_path, const std::string& api_url)
    : config_path_(device_config_path), api_url_(api_url) {}

void DeviceSync::registerWithCloud(const std::string& device_id, const std::string& device_secret, const std::vector<std::string>& team_members) {
    if (team_members.empty()) {
        printf("Device Sync: no team members, skipping registration\n");
        return;
    }

    std::string url = api_url_ + "/api/devices/register";

    nlohmann::json body;
    body["teamMembers"] = team_members;

    ix::HttpClient client;
    auto args = client.createRequest();
    args->extraHeaders["X-Device-ID"] = device_id;
    args->extraHeaders["X-Device-Secret"] = device_secret;
    args->extraHeaders["Content-Type"] = "application/json";
    args->body = body.dump();

    auto response = client.post(url, args->body, args);
    if (response->statusCode == 200) {
        printf("Device Sync: registered with cloud\n");
    } else {
        printf("Device Sync: registration failed: %d %s\n", response->statusCode, response->body.c_str());
    }
}

bool DeviceSync::handleTeamMembersUpdate(const std::string& msg, std::vector<std::string>& team_members_out) {
    nlohmann::json j;
    try {
        j = nlohmann::json::parse(msg);
    } catch (...) {
        return false;
    }

    if (j.value("type", "") != "team_members_update") return false;
    if (!j.contains("team_members") || !j["team_members"].is_array()) return false;

    // parse the new team members list
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
        return true; // message was handled, just failed to persist
    }
    out << config.dump(2);
    out.close();
    printf("Device Sync: updated team_members in %s\n", config_path_.c_str());

    team_members_out = std::move(members);
    return true;
}
