/**
 * device_sync.hpp      Device Config Sync (cloud → Pi)
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

    // Push device identity + team members to cloud (Pi to cloud)
    void registerWithCloud(const std::string& device_id, const std::string& device_secret, const std::vector<std::string>& team_members);

    // Handle incoming team_members_update (cloud to Pi)
    // Returns true if the message was handled
    bool handleTeamMembersUpdate(const std::string& msg, std::vector<std::string>& team_members_out);

private:
    std::string config_path_;
    std::string api_url_;
};

#endif
