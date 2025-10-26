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
    
    // Create flatbuffer message
    auto message = createFlatbufferMessage(colors);
    
    // Send message
    return sendMessage(message.data(), message.size());
}

bool HyperHDRClient::sendMessage(const uint8_t* data, size_t size) {
    // Send size header (4 bytes, big endian)
    uint32_t msg_size = htonl(static_cast<uint32_t>(size));
    
    ssize_t sent = send(socket_fd_, &msg_size, sizeof(msg_size), 0);
    if (sent != sizeof(msg_size)) {
        LOG_ERROR("Failed to send message size header");
        return false;
    }
    
    // Send actual data
    sent = send(socket_fd_, data, size, 0);
    if (sent != static_cast<ssize_t>(size)) {
        LOG_ERROR("Failed to send message data");
        return false;
    }
    
    return true;
}

std::vector<uint8_t> HyperHDRClient::createFlatbufferMessage(const std::vector<cv::Vec3b>& colors) {
    // For now, we'll create a simple protocol buffer-style message
    // TODO: Replace with actual HyperHDR Flatbuffer schema when available
    
    // Simple format: command byte + priority (4 bytes) + LED count (4 bytes) + RGB data
    std::vector<uint8_t> message;
    
    // Command: 0x01 = set colors
    message.push_back(0x01);
    
    // Priority (4 bytes, big endian)
    uint32_t priority_be = htonl(static_cast<uint32_t>(priority_));
    const uint8_t* priority_bytes = reinterpret_cast<const uint8_t*>(&priority_be);
    message.insert(message.end(), priority_bytes, priority_bytes + 4);
    
    // LED count (4 bytes, big endian)
    uint32_t led_count_be = htonl(static_cast<uint32_t>(colors.size()));
    const uint8_t* count_bytes = reinterpret_cast<const uint8_t*>(&led_count_be);
    message.insert(message.end(), count_bytes, count_bytes + 4);
    
    // RGB data (3 bytes per LED)
    for (const auto& color : colors) {
        message.push_back(color[0]);  // R
        message.push_back(color[1]);  // G
        message.push_back(color[2]);  // B
    }
    
    return message;
}

} // namespace TVLED

