#include "communication/HyperHDRClient.h"
#include "utils/Logger.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <sstream>

namespace TVLED {

HyperHDRClient::HyperHDRClient(const std::string& host, int port, int priority)
    : host_(host), port_(port), priority_(priority), connected_(false), socket_fd_(-1) {
    std::memset(&server_addr_, 0, sizeof(server_addr_));
}

HyperHDRClient::~HyperHDRClient() {
    disconnect();
}

bool HyperHDRClient::connect() {
    if (connected_) {
        LOG_WARN("Already initialized UDP connection to HyperHDR");
        return true;
    }
    
    // Create UDP socket
    socket_fd_ = socket(AF_INET, SOCK_DGRAM, 0);
    if (socket_fd_ < 0) {
        LOG_ERROR("Failed to create UDP socket");
        return false;
    }
    
    // Setup server address
    server_addr_.sin_family = AF_INET;
    server_addr_.sin_port = htons(port_);
    
    if (inet_pton(AF_INET, host_.c_str(), &server_addr_.sin_addr) <= 0) {
        LOG_ERROR("Invalid address: " + host_);
        close(socket_fd_);
        socket_fd_ = -1;
        return false;
    }
    
    connected_ = true;
    LOG_INFO("UDP connection initialized for HyperHDR at " + host_ + ":" + std::to_string(port_));
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
    
    // Create simple UDP message for individual LED control
    auto message = createUDPMessage(colors);
    
    // Send message via UDP
    return sendUDPMessage(message.data(), message.size());
}

bool HyperHDRClient::sendUDPMessage(const uint8_t* data, size_t size) {
    ssize_t sent = sendto(socket_fd_, data, size, 0,
                          (struct sockaddr*)&server_addr_, sizeof(server_addr_));
    if (sent != static_cast<ssize_t>(size)) {
        LOG_ERROR("Failed to send UDP message");
        return false;
    }
    
    return true;
}

std::vector<uint8_t> HyperHDRClient::createUDPMessage(const std::vector<cv::Vec3b>& colors) {
    // HyperHDR UDP protocol is simple: just send raw RGB data
    // Format: RGBRGBRGB... (3 bytes per LED)
    // HyperHDR automatically maps this to your LED layout
    
    std::vector<uint8_t> message;
    message.reserve(colors.size() * 3);
    
    // Build the raw LED color data (RGB triplets)
    for (const auto& color : colors) {
        message.push_back(color[0]);  // R
        message.push_back(color[1]);  // G
        message.push_back(color[2]);  // B
    }
    
    LOG_INFO("Sending " + std::to_string(colors.size()) + " individual LED colors to HyperHDR via UDP");
    
    return message;
}

} // namespace TVLED

