/**
 * log_uploader.cpp     Log File Uploader
 *
 * @author      Cameron Stone '26 <cameron28202@gmail.com>
 *
 * @copyright   Texas A&M University
 */

#include "log_uploader.hpp"

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>

#include <ixwebsocket/IXHttpClient.h>

namespace fs = std::filesystem;

static constexpr int POLL_INTERVAL_S = 10;

LogUploader::LogUploader(const std::string& log_dir,
                         const std::string& upload_url,
                         const std::string& device_id)
    : log_dir_(log_dir), upload_url_(upload_url), device_id_(device_id) {}

LogUploader::~LogUploader() {
    stop();
}

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
    while (running_.load()) {
        for (int i = 0; i < POLL_INTERVAL_S && running_.load(); ++i){
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        if (!running_.load()) break;
        if (upload_url_.empty()) continue;

        try {
            if(!fs::exists(log_dir_)) continue;

            for (const auto& entry : fs::directory_iterator(log_dir_)) {
                if (!running_.load()) break;
                if (!entry.is_regular_file()) continue;
                if (entry.path().extension() != ".bin") continue;

                std::string filename = entry.path().filename().string();
                std::string filepath = entry.path().string();

                if (upload_file(filepath, filename)) {
                    std::error_code ec;
                    fs::remove(filepath, ec);
                    if (!ec) {
                        printf("log uploader: uploaded and deleted %s\n", filename.c_str());
                    }
                }
            }
        } catch (const fs::filesystem_error& e) {
            printf("log uploader: error: %s\n", e.what());
        }
    }
}

bool LogUploader::upload_file(const std::string& path, const std::string& filename) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) return false;

    auto size = file.tellg();
    file.seekg(0);
    std::string body(size, '\0');
    file.read(body.data(), size);
    file.close();

    std::string url = upload_url_ + "/" + device_id_ + "/" + filename;

    ix::HttpClient client;
    auto args = client.createRequest(url, ix::HttpClient::kPut);
    args->extraHeaders["X-Device-ID"] = device_id_;
    args->extraHeaders["Content-Type"] = "application/octet-stream";
    args->connectTimeout = 10;
    args->transferTimeout = 60;

    auto response = client.put(url, body, args);
    if (response && response->statusCode == 200) return true;

    printf("[log-upload] failed %s: HTTP %d\n",
           filename.c_str(), response ? response->statusCode : 0);
    return false;
}
