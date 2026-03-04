#include "log_uploader.hpp"

#include <filesystem>

LogUploader::LogUploader(const std::string& log_dir) : log_dir_(log_dir) {}

LogUploader::~LogUploader() {
    stop();
}

void LogUploader::start() {
    running_.store(true);
    upload_thread_ = std::thread(&LogUploader::upload_loop, this);
}

void LogUploader::stop() {
    running_.store(false);
    if (upload_thread_.joinable()){
        upload_thread_.join();
    }
}

void LogUploader::upload_loop() {
    while (running_.load()) {
        // watch log_dir_ for new files and upload them

        for(const auto& entry : std::filesystem::directory_iterator(log_dir_)) {
            if (entry.is_regular_file()) {
                const auto& path = entry.path();
                // check if file is new and not currently being written to
                // if so, upload to cloud and delete local file
                // todo: implement
                if(/* file is new and ready to upload */true) {
                    // upload file to cloud
                    // if upload successful, delete file
                }
            }
        }

    }

}