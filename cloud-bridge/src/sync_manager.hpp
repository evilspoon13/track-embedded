/**
 * sync_manager.hpp    Bidirectional config sync (Pi ↔ cloud)
 *
 * v1: sync /opt/track/config/graphics.json with cloud coordinator.
 *
 * @author      Brayden Bailey '26
 * 
 * @copyright   Texas A&M University
 */

#ifndef TRACK_SYNC_MANAGER_HPP
#define TRACK_SYNC_MANAGER_HPP

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>

class WsClient;

class SyncManager {
public:
    struct Options {
        std::string file_id;      // e.g. "graphics.json"
        std::string file_path;    // e.g. "/opt/track/config/graphics.json"
        std::string state_path;   // e.g. "/opt/track/state/sync_state.json"
        std::string reload_process; // e.g. "graphics-engine" or "can-reader"
    };

    SyncManager(const Options& opts, WsClient& ws, std::string device_id);
    ~SyncManager();

    SyncManager(const SyncManager&) = delete;
    SyncManager& operator=(const SyncManager&) = delete;

    void start();
    void stop();

    void set_device_id(std::string device_id);

    // Called by main loop when WS transitions to connected.
    void on_ws_connected();

    // Called by main loop when WS transitions to disconnected.
    void on_ws_disconnected();

    // Returns true if message was handled by sync.
    bool handle_message(const std::string& msg);

private:
    struct State {
        std::uint64_t version_id = 0;
        bool pending_upload = false;
        std::string pending_change_id;
        std::uint64_t pending_base_version_id = 0;
    };

    void load_state_locked();
    void save_state_locked();
    void ensure_state_dir_locked();

    void attempt_upload_locked();
    void apply_download_locked(std::uint64_t version_id,
                              const std::string& content_b64);

    static std::string gen_change_id();
    static bool read_file_bytes(const std::string& path, std::string& out);
    static bool write_file_bytes(const std::string& path, const std::string& bytes);

    Options opts_;
    WsClient& ws_;

    std::mutex mu_;
    std::string device_id_;
    State state_;
    bool hello_sent_for_connection_ = false;
};

#endif
