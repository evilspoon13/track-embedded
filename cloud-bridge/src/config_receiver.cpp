/**
 * config_receiver.cpp  Config Receiver
 *
 * @author      Cameron Stone '26 <cameron28202@gmail.com>
 *
 * @copyright   Texas A&M University
 */

#include "config_receiver.hpp"

#include <cstdio>
#include <csignal>
#include <fstream>

#include <nlohmann/json.hpp>

ConfigReceiver::ConfigReceiver(const std::string& config_path) : config_path_(config_path) {}

// helper to find the process ID to send the SIGHUP
static pid_t find_process(const char* name) {
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "pidof %s 2>/dev/null", name);
    FILE* fp = popen(cmd, "r");
    if (!fp) return 0;

    pid_t pid = 0;
    if (fscanf(fp, "%d", &pid) != 1) pid = 0;
    pclose(fp);
    return pid;
}

void ConfigReceiver::ReceiveCallback(const std::string& msg) {
    nlohmann::json j;
    try {
        j = nlohmann::json::parse(msg);
    } catch (const nlohmann::json::parse_error& e) {
        printf("failed to parse: %s\n", e.what());
        return;
    }


    if (j.value("type", "") != "config_update")return;

    if (!j.contains("payload")) {
        printf("config_update missing payload\n");
        return;
    }

    // write payload to config file
    std::ofstream f(config_path_);
    if (!f.is_open()) {
        printf("failed to write %s\n", config_path_.c_str());
        return;
    }
    f << j["payload"].dump(2);
    f.close();

    printf("wrote config to %s\n", config_path_.c_str());

    // signal graphics engine to reload
    pid_t pid = find_process("graphics-engine");
    if (pid > 0) {
        kill(pid, SIGHUP);
        printf("sent SIGHUP to graphics-engine (pid %d)\n", pid);
    }
    else {
        printf("graphics-engine not running, skipping SIGHUP\n");
    }
}