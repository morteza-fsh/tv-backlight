#include "communication/HyperHDRClient.h"
#include "utils/Logger.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <sstream>
#include <iomanip>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace TVLED {

HyperHDRClient::HyperHDRClient(const std::string& host, int port, int priority)
    : host_(host), port_(port), priority_(priority), connected_(false), socket_fd_(-1) {
}

HyperHDRClient::~HyperHDRClient() {
    disconnect();
}

bool HyperHDRClient::connect() {
    if (connected_) {
        LOG_WARN("Already connected to HyperHDR");
        return true;
    }
    
    socket_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd_ < 0) {
        LOG_ERROR("Failed to create socket");
        return false;
    }
    
    struct sockaddr_in server_addr;
    std::memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port_);
    
    if (inet_pton(AF_INET, host_.c_str(), &server_addr.sin_addr) <= 0) {
        LOG_ERROR("Invalid address: " + host_);
        close(socket_fd_);
        socket_fd_ = -1;
        return false;
    }
    
    if (::connect(socket_fd_, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        LOG_ERROR("Failed to connect to HyperHDR at " + host_ + ":" + std::to_string(port_));
        close(socket_fd_);
        socket_fd_ = -1;
        return false;
    }
    
    connected_ = true;
    LOG_INFO("Connected to HyperHDR at " + host_ + ":" + std::to_string(port_));
    return true;
}

void HyperHDRClient::disconnect() {
    if (socket_fd_ >= 0) {
        close(socket_fd_);
        socket_fd_ = -1;
    }
    connected_ = false;
    LOG_INFO("Disconnected from HyperHDR");
}

bool HyperHDRClient::sendColors(const std::vector<cv::Vec3b>& colors) {
    if (!connected_) {
        LOG_ERROR("Not connected to HyperHDR");
        return false;
    }
    
    if (colors.empty()) {
        LOG_WARN("No colors to send");
        return false;
    }
    
    // Create JSON message (simpler and more reliable than flatbuffer)
    auto message = createJsonMessage(colors);
    
    // Send message
    return sendMessage(reinterpret_cast<const uint8_t*>(message.c_str()), message.size());
}

bool HyperHDRClient::sendMessage(const uint8_t* data, size_t size) {
    // HyperHDR expects messages in the format: size (4 bytes, big endian) + data
    // But we're sending raw data without the size header, as HyperHDR's flatbuffer protocol handles it
    ssize_t sent = send(socket_fd_, data, size, 0);
    if (sent != static_cast<ssize_t>(size)) {
        LOG_ERROR("Failed to send message data");
        return false;
    }
    
    return true;
}

std::string HyperHDRClient::createJsonMessage(const std::vector<cv::Vec3b>& colors) {
    // HyperHDR JSON-RPC API for setting LED colors
    // This is much simpler and more reliable than the flatbuffer protocol
    
    json message;
    message["command"] = "color";
    message["priority"] = priority_;
    message["origin"] = "TV LED Controller";
    
    // Build LED data array
    json led_data = json::array();
    for (const auto& color : colors) {
        json led_color = json::array();
        led_color.push_back(color[0]);  // R
        led_color.push_back(color[1]);  // G
        led_color.push_back(color[2]);  // B
        led_data.push_back(led_color);
    }
    message["ledColors"] = led_data;
    
    // Convert to string with newline terminator (required by HyperHDR JSON protocol)
    std::string json_str = message.dump() + "\n";
    return json_str;
}

} // namespace TVLED

