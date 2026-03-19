/**
 * log_uploader.hpp     Log File Uploader
 *
 * @author      Cameron Stone '26 <cameron28202@gmail.com>
 *
 * @copyright   Texas A&M University
 */

#ifndef _LOG_UPLOADER_HPP
#define _LOG_UPLOADER_HPP

// on background thread, watch a directory for new log files and upload them to the cloud when we can

#include <thread>
#include <string>
#include <atomic>

class LogUploader {
public:
    LogUploader(const std::string& log_dir);
    ~LogUploader();

    void start();
    void stop();
private:
    std::string log_dir_;
    std::thread upload_thread_;
    std::atomic<bool> running_{false};

    void upload_loop();
};

#endif