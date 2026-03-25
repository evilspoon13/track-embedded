/**
 * log_uploader.hpp     Log File Uploader
 *
 * @author      Cameron Stone '26 <cameron28202@gmail.com>
 *
 * @copyright   Texas A&M University
 */

#ifndef _LOG_UPLOADER_HPP
#define _LOG_UPLOADER_HPP

#include <thread>
#include <string>
#include <atomic>

class LogUploader {
public:
    LogUploader(const std::string& log_dir,
                const std::string& upload_url,
                const std::string& device_id);
    ~LogUploader();

    void start();
    void stop();
private:
    std::string log_dir_;
    std::string upload_url_;
    std::string device_id_;
    std::thread upload_thread_;
    std::atomic<bool> running_{false};

    void upload_loop();
    bool upload_file(const std::string& path, const std::string& filename);
};

#endif
