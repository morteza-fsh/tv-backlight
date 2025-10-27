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
    // HyperHDR Proto-v1 format for LED colors
    // Header format: 
    // - 4 bytes: type identification ("PRIO" = 0x5052494F)
    // - 1 byte: version (0x01)
    // - 4 bytes: priority (big endian)
    // - 4 bytes: duration_ms (0 = infinite)
    // - 4 bytes: priority_timeout (0 = infinite)
    // - 4 bytes: origin (0 = manual)
    // - 4 bytes: data size (big endian)
    // - Variable: LED data (RGB triplets)
    
    std::vector<uint8_t> message;
    
    // HyperHDR header: type "PRIO" (0x5052494F)
    message.push_back(0x50); // 'P'
    message.push_back(0x52); // 'R'
    message.push_back(0x49); // 'I'
    message.push_back(0x4F); // 'O'
    
    // Version (1)
    message.push_back(0x01);
    
    // Priority (4 bytes, big endian)
    uint32_t priority_be = htonl(static_cast<uint32_t>(priority_));
    const uint8_t* priority_bytes = reinterpret_cast<const uint8_t*>(&priority_be);
    message.insert(message.end(), priority_bytes, priority_bytes + 4);
    
    // Duration (4 bytes, 0 = infinite)
    uint32_t duration = 0;
    uint32_t duration_be = htonl(duration);
    const uint8_t* duration_bytes = reinterpret_cast<const uint8_t*>(&duration_be);
    message.insert(message.end(), duration_bytes, duration_bytes + 4);
    
    // Priority timeout (4 bytes, 0 = infinite)
    uint32_t timeout = 0;
    uint32_t timeout_be = htonl(timeout);
    const uint8_t* timeout_bytes = reinterpret_cast<const uint8_t*>(&timeout_be);
    message.insert(message.end(), timeout_bytes, timeout_bytes + 4);
    
    // Origin (4 bytes, 0 = manual)
    uint32_t origin = 0;
    uint32_t origin_be = htonl(origin);
    const uint8_t* origin_bytes = reinterpret_cast<const uint8_t*>(&origin_be);
    message.insert(message.end(), origin_bytes, origin_bytes + 4);
    
    // LED count (4 bytes, big endian)
    uint32_t led_count = static_cast<uint32_t>(colors.size());
    uint32_t led_count_be = htonl(led_count);
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

