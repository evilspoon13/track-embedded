/**
 * log_uploader.cpp     Log File Uploader
 *
 * @author      Cameron Stone '26 <cameron28202@gmail.com>
 *
 * @copyright   Texas A&M University
 */

#include "log_uploader.hpp"

#include <cstdio>
#include <filesystem>
#include <string>
#include <thread>

#include <ixwebsocket/IXBase64.h>
#include <nlohmann/json.hpp>

#include "time_util.hpp"

static constexpr size_t CHUNK_SIZE = 65536; // 64 KB

LogUploader::LogUploader(const std::string& log_dir, WsClient& ws, const std::string& device_id)
    : log_dir_(log_dir), ws_(ws), device_id_(device_id) {}

LogUploader::~LogUploader() { stop(); }

void LogUploader::start() {
    running_.store(true);
    upload_thread_ = std::thread(&LogUploader::upload_loop, this);
}

void LogUploader::stop() {
    running_.store(false);
    if (upload_thread_.joinable()) {
        upload_thread_.join();
    }
}

void LogUploader::upload_loop() {
    namespace fs = std::filesystem;

    while (running_.load()) {
        if (ws_.is_connected()) {
            for (const auto& entry : fs::directory_iterator(log_dir_)) {
                if (!running_.load()) break;
                if (!entry.is_regular_file()) continue;

                const auto& path = entry.path();
                if (path.extension() != ".done") continue;

                fs::path bin_path = path;
                bin_path.replace_extension(".bin");

                if (!fs::exists(bin_path)) {
                    fs::remove(path);
                    continue;
                }

                upload_file(bin_path, path);
            }
        }

        // Interruptible 5-second sleep: check running_ every 100ms
        for (int i = 0; i < 50 && running_.load(); ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
}

void LogUploader::upload_file(const std::filesystem::path& bin_path,
                               const std::filesystem::path& done_path) {
    namespace fs = std::filesystem;

    std::string filename = bin_path.filename().string();
    uintmax_t file_size = fs::file_size(bin_path);

    // Send start — backend resets any prior partial transfer for this filename
    {
        nlohmann::json j;
        j["type"] = "log_upload_start";
        j["device_id"] = device_id_;
        j["filename"] = filename;
        j["file_size"] = file_size;
        j["ts"] = now_ms();
        ws_.send(j.dump());
    }

    FILE* f = fopen(bin_path.c_str(), "rb");
    if (!f) {
        // log_upload_start already sent; backend will reset on next retry's start message
        std::perror("[uploader] fopen failed");
        return;
    }

    std::string buf(CHUNK_SIZE, '\0');
    int chunk_index = 0;

    while (true) {
        size_t n = fread(buf.data(), 1, CHUNK_SIZE, f);
        if (n == 0) break;

        nlohmann::json j;
        j["type"] = "log_upload_chunk";
        j["filename"] = filename;
        j["chunk_index"] = chunk_index++;
        j["data"] = macaron::Base64::Encode(buf.substr(0, n));
        j["ts"] = now_ms();
        ws_.send(j.dump());

        if (!running_.load() || !ws_.is_connected()) {
            fclose(f);
            printf("[uploader] aborted %s after chunk %d\n", filename.c_str(), chunk_index - 1);
            return;
        }
    }

    fclose(f);

    // Send end
    {
        nlohmann::json j;
        j["type"] = "log_upload_end";
        j["device_id"] = device_id_;
        j["filename"] = filename;
        j["total_chunks"] = chunk_index;
        j["ts"] = now_ms();
        ws_.send(j.dump());
    }

    // Only delete after log_upload_end is sent
    fs::remove(bin_path);
    fs::remove(done_path);

    printf("[uploader] uploaded %s (%d chunks)\n", filename.c_str(), chunk_index);
}
