#include "ws_client.hpp"

#include <cstdio>

WsClient::WsClient(const std::string& url, const std::string& device_id, const std::string& device_secret) {

    // blalblaalbla
    ws_.setUrl(url);

    ix::WebSocketHttpHeaders headers;
    headers["X-Device-ID"] = device_id;
    headers["X-Device-Secret"] = device_secret;
    ws_.setExtraHeaders(headers);

    ws_.setPingInterval(30);
    ws_.enableAutomaticReconnection();
    ws_.setMinWaitBetweenReconnectionRetries(1000);
    ws_.setMaxWaitBetweenReconnectionRetries(30000);
}

WsClient::~WsClient() {
    stop();
}

void WsClient::start() {
    ws_.setOnMessageCallback([this](const ix::WebSocketMessagePtr& msg) {
        switch (msg->type) {
            case ix::WebSocketMessageType::Open:
                connected_ = true;
                printf("connected to %s\n", msg->openInfo.uri.c_str());
                break;

            case ix::WebSocketMessageType::Close:
                connected_ = false;
                printf("disconnected: %s (code %d)\n", msg->closeInfo.reason.c_str(), msg->closeInfo.code);
                break;

            case ix::WebSocketMessageType::Error:
                connected_ = false;
                printf("error: %s\n", msg->errorInfo.reason.c_str());
                break;

            case ix::WebSocketMessageType::Message: {
                std::lock_guard<std::mutex> lock(cb_mutex_);
                if (on_message_) {
                    on_message_(msg->str);
                }
                break;
            }

            default:
                break;
        }
    });

    ws_.start();
    printf("connecting...\n");
}

void WsClient::stop() {
    ws_.stop();
    connected_ = false;
}

bool WsClient::is_connected() const {
    return connected_.load();
}

bool WsClient::send(const std::string& json){
    if (!connected_.load()) return false;
    return ws_.send(json).success;
}

void WsClient::set_on_message(MessageCallback cb){
    std::lock_guard<std::mutex> lock(cb_mutex_);
    on_message_ = std::move(cb);
    // release
}
