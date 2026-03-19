/**
 * config_receiver.hpp  Config Receiver
 *
 * @author      Cameron Stone '26 <cameron28202@gmail.com>
 *
 * @copyright   Texas A&M University
 */

#ifndef _CONFIG_RECEIVER_HPP
#define _CONFIG_RECEIVER_HPP

#include <string>

class ConfigReceiver {
public:
    ConfigReceiver(const std::string& config_path);
    ~ConfigReceiver() = default;

    void ReceiveCallback(const std::string& msg);

private:
    std::string config_path_;
};

#endif