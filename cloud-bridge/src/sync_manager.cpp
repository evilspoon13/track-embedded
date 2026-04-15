/**
 * sync_manager.cpp    Bidirectional config sync (Pi ↔ cloud)
 *
 * @author      Brayden Bailey '26
 * 
 * @copyright   Texas A&M University
 */

#include "sync_manager.hpp"

#include <array>
#include <csignal>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <random>

#include <ixwebsocket/IXBase64.h>
#include <nlohmann/json.hpp>

#include "ws_client.hpp"

static bool read_file_bytes_local(const std::string& path, std::string& out) {
    std::ifstream in(path, std::ios::binary);
    if (!in.is_open()) return false;
    in.seekg(0, std::ios::end);
    std::streamoff sz = in.tellg();
    if (sz < 0) return false;
    in.seekg(0, std::ios::beg);
    out.resize(static_cast<size_t>(sz));
    if (sz > 0) in.read(out.data(), sz);
    return in.good() || in.eof();
}

SyncManager::SyncManager(const Options& opts, WsClient& ws, std::string device_id)
    : opts_(opts), ws_(ws), device_id_(std::move(device_id)) {}

SyncManager::~SyncManager() { stop(); }

void SyncManager::start() {
    // No-op (sync is triggered by WS reconnect + explicit reload signals).
}

void SyncManager::stop() {
    // No-op.
}

void SyncManager::set_device_id(std::string device_id) {
    std::lock_guard<std::mutex> lock(mu_);
    device_id_ = std::move(device_id);
}

void SyncManager::on_ws_connected() {
    std::lock_guard<std::mutex> lock(mu_);
    hello_sent_for_connection_ = false;

    load_state_locked();

    if (hello_sent_for_connection_) return;

    nlohmann::json hello;
    hello["type"] = "sync_state";
    hello["device_id"] = device_id_;
    hello["protocol"] = 1;
    nlohmann::json file;
    file["file_id"] = opts_.file_id;
    file["version_id"] = state_.version_id;
    file["pending_upload"] = state_.pending_upload;
    if (state_.pending_upload) {
        file["pending_base_version_id"] = state_.pending_base_version_id;
        file["pending_change_id"] = state_.pending_change_id;
    }
    hello["files"] = nlohmann::json::array({file});

    if (ws_.send(hello.dump())) {
        hello_sent_for_connection_ = true;
    }
}

void SyncManager::on_ws_disconnected() {
    std::lock_guard<std::mutex> lock(mu_);
    hello_sent_for_connection_ = false;
}

static bool json_is_sync_type(const nlohmann::json& j, const char* type) {
    return j.is_object() && j.value("type", "") == type;
}

bool SyncManager::handle_message(const std::string& msg) {
    nlohmann::json j;
    try {
        j = nlohmann::json::parse(msg);
    } catch (...) {
        return false;
    }

    if (!j.is_object()) return false;

    const std::string type = j.value("type", "");
    if (type != "sync_plan" && type != "sync_download" && type != "sync_commit" && type != "sync_reject") {
        return false;
    }

    const std::string file_id = j.value("file_id", "");
    if (file_id != opts_.file_id) return false;

    std::lock_guard<std::mutex> lock(mu_);
    load_state_locked();

    if (type == "sync_plan") {
        const std::string action = j.value("action", "");
        if (action == "noop") {
            return true;
        }
        if (action == "request_upload") {
            attempt_upload_locked();
            return true;
        }
        if (action == "send_download") {
            if (!j.contains("download") || !j["download"].is_object()) return true;
            const auto& d = j["download"];
            apply_download_locked(
                d.value("version_id", 0ull),
                d.value("content_b64", std::string{})
            );
            return true;
        }
        return true;
    }

    if (type == "sync_download") {
        apply_download_locked(
            j.value("version_id", 0ull),
            j.value("content_b64", std::string{})
        );
        return true;
    }

    if (type == "sync_commit") {
        const std::string change_id = j.value("change_id", "");
        const std::uint64_t version_id = j.value("version_id", 0ull);
        if (!change_id.empty() && change_id == state_.pending_change_id) {
            state_.version_id = version_id;
            state_.pending_upload = false;
            state_.pending_change_id.clear();
            state_.pending_base_version_id = 0;
            save_state_locked();
        }
        return true;
    }

    if (type == "sync_reject") {
        if (j.contains("download") && j["download"].is_object()) {
            const auto& d = j["download"];
            apply_download_locked(
                d.value("version_id", 0ull),
                d.value("content_b64", std::string{})
            );
        }
        return true;
    }

    return false;
}

void SyncManager::load_state_locked() {
    state_ = State{};
    std::ifstream in(opts_.state_path);
    if (!in.is_open()) return;
    nlohmann::json j = nlohmann::json::parse(in, nullptr, false);
    if (!j.is_object()) return;
    state_.version_id = j.value("version_id", 0ull);
    state_.pending_upload = j.value("pending_upload", false);
    state_.pending_change_id = j.value("pending_change_id", std::string{});
    state_.pending_base_version_id = j.value("pending_base_version_id", 0ull);
}

void SyncManager::ensure_state_dir_locked() {
    namespace fs = std::filesystem;
    fs::path p(opts_.state_path);
    auto dir = p.parent_path();
    if (dir.empty()) return;
    std::error_code ec;
    fs::create_directories(dir, ec);
}

void SyncManager::save_state_locked() {
    ensure_state_dir_locked();
    nlohmann::json j;
    j["version_id"] = state_.version_id;
    j["pending_upload"] = state_.pending_upload;
    j["pending_change_id"] = state_.pending_change_id;
    j["pending_base_version_id"] = state_.pending_base_version_id;

    std::ofstream out(opts_.state_path);
    if (!out.is_open()) {
        printf("[sync] failed to write state %s\n", opts_.state_path.c_str());
        return;
    }
    out << j.dump(2);
}

std::string SyncManager::gen_change_id() {
    std::array<unsigned char, 16> bytes{};
    std::random_device rd;
    for (auto& b : bytes) b = static_cast<unsigned char>(rd());
    static const char* kHex = "0123456789abcdef";
    std::string out;
    out.reserve(32);
    for (unsigned char b : bytes) {
        out.push_back(kHex[(b >> 4) & 0x0F]);
        out.push_back(kHex[b & 0x0F]);
    }
    return out;
}

bool SyncManager::read_file_bytes(const std::string& path, std::string& out) {
    std::ifstream in(path, std::ios::binary);
    if (!in.is_open()) return false;
    in.seekg(0, std::ios::end);
    std::streamoff sz = in.tellg();
    if (sz < 0) return false;
    in.seekg(0, std::ios::beg);
    out.resize(static_cast<size_t>(sz));
    if (sz > 0) in.read(out.data(), sz);
    return in.good() || in.eof();
}

bool SyncManager::write_file_bytes(const std::string& path, const std::string& bytes) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out.is_open()) return false;
    out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    out.flush();
    return out.good();
}

void SyncManager::attempt_upload_locked() {
    if (!state_.pending_upload) return;
    if (!ws_.is_connected()) return;

    std::string bytes;
    if (!read_file_bytes_local(opts_.file_path, bytes)) {
        printf("[sync] upload failed: cannot read %s\n", opts_.file_path.c_str());
        return;
    }

    nlohmann::json up;
    up["type"] = "sync_upload";
    up["device_id"] = device_id_;
    up["file_id"] = opts_.file_id;
    up["base_version_id"] = state_.pending_base_version_id;
    up["change_id"] = state_.pending_change_id;
    up["content_b64"] = macaron::Base64::Encode(bytes);

    if (ws_.send(up.dump())) {
        printf("[sync] uploaded pending change %s (base=%llu)\n",
               state_.pending_change_id.c_str(),
               static_cast<unsigned long long>(state_.pending_base_version_id));
    }
}

void SyncManager::apply_download_locked(std::uint64_t version_id,
                                       const std::string& content_b64) {
    if (content_b64.empty()) return;

    const std::string bytes = macaron::Base64::Decode(content_b64);
    if (!write_file_bytes(opts_.file_path, bytes)) {
        printf("[sync] download failed: cannot write %s\n", opts_.file_path.c_str());
        return;
    }

    // Signal graphics engine to reload (same behavior as config_update path).
    if (!opts_.reload_process.empty()) {
        auto find_process = [](const char* name) -> pid_t {
            char cmd[256];
            std::snprintf(cmd, sizeof(cmd), "pidof %s 2>/dev/null", name);
            FILE* fp = popen(cmd, "r");
            if (!fp) return 0;
            pid_t pid = 0;
            if (std::fscanf(fp, "%d", &pid) != 1) pid = 0;
            pclose(fp);
            return pid;
        };
        pid_t pid = find_process(opts_.reload_process.c_str());
        if (pid > 0) {
            kill(pid, SIGHUP);
        }
    }

    state_.version_id = version_id;
    state_.pending_upload = false;
    state_.pending_change_id.clear();
    state_.pending_base_version_id = 0;
    save_state_locked();

    printf("[sync] applied download version_id=%llu\n", static_cast<unsigned long long>(version_id));
}
