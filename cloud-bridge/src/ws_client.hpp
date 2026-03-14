#ifndef TRACK_WS_CLIENT_HPP
#define TRACK_WS_CLIENT_HPP

#include <atomic>
#include <functional>
#include <mutex>
#include <string>

#include <ixwebsocket/IXWebSocket.h>

class WsClient {
public:
    using MessageCallback = std::function<void(const std::string&)>;

    WsClient(const std::string& url, const std::string& device_id, const std::string& device_secret);

    ~WsClient();

    WsClient(const WsClient&) = delete;

    WsClient& operator=(const WsClient&) = delete;

    void start();
    void stop();
    bool is_connected() const;

    void send(const std::string& json);
    void set_on_message(MessageCallback cb);

private:
    ix::WebSocket ws_;
    std::atomic<bool> connected_{false};
    MessageCallback on_message_;
    std::mutex cb_mutex_;
};

#endif