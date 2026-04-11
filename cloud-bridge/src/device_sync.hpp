/**
 * device_sync.hpp      Device Identity & Config Sync (cloud → Pi)
 *
 * @author      Cameron Stone '26 <cameron28202@gmail.com>
 *
 * @copyright   Texas A&M University
 */

#ifndef _DEVICE_SYNC_HPP
#define _DEVICE_SYNC_HPP

#include <string>
#include <vector>

class DeviceSync {
public:
    DeviceSync(const std::string& device_config_path, const std::string& api_url);
    ~DeviceSync() = default;

    // Reload device.json from disk (call on SIGHUP)
    void reload();

    // Push device identity + team members to cloud
    void registerWithCloud();

    // Handle incoming team_members_update (cloud to Pi)
    // Returns true if the message was handled
    bool handleTeamMembersUpdate(const std::string& msg);

    const std::string& device_id() const { return device_id_; }
    const std::string& device_secret() const { return device_secret_; }
    const std::vector<std::string>& team_members() const { return team_members_; }

private:
    void load_identity();

    std::string config_path_;
    std::string api_url_;
    std::string device_id_;
    std::string device_secret_;
    std::vector<std::string> team_members_;
};

#endif
