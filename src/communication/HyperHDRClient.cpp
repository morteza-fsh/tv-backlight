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
    // HyperHDR expects messages in the format: size (4 bytes, big endian) + data
    // But we're sending raw data without the size header, as HyperHDR's flatbuffer protocol handles it
    ssize_t sent = send(socket_fd_, data, size, 0);
    if (sent != static_cast<ssize_t>(size)) {
        LOG_ERROR("Failed to send message data");
        return false;
    }
    
    return true;
}

std::vector<uint8_t> HyperHDRClient::createFlatbufferMessage(const std::vector<cv::Vec3b>& colors) {
    // HyperHDR uses a flatbuffer protocol for LED colors
    // The protocol expects:
    // 1. Size header (4 bytes, big endian) - total message size
    // 2. Message type identifier (4 bytes) - "PRIO" = 0x5052494F
    // 3. Version (1 byte) - protocol version (1)
    // 4. Priority (4 bytes, big endian)
    // 5. Duration (4 bytes, big endian, 0 = infinite)
    // 6. Timeout (4 bytes, big endian, 0 = infinite)
    // 7. Origin (4 bytes, big endian, 0 = manual)
    // 8. LED count (4 bytes, big endian)
    // 9. RGB data (3 bytes per LED)
    
    std::vector<uint8_t> payload;
    
    // HyperHDR header: type "PRIO" (0x5052494F)
    payload.push_back(0x50); // 'P'
    payload.push_back(0x52); // 'R'
    payload.push_back(0x49); // 'I'
    payload.push_back(0x4F); // 'O'
    
    // Version (1)
    payload.push_back(0x01);
    
    // Priority (4 bytes, big endian)
    uint32_t priority_be = htonl(static_cast<uint32_t>(priority_));
    const uint8_t* priority_bytes = reinterpret_cast<const uint8_t*>(&priority_be);
    payload.insert(payload.end(), priority_bytes, priority_bytes + 4);
    
    // Duration (4 bytes, 0 = infinite)
    uint32_t duration = 0;
    uint32_t duration_be = htonl(duration);
    const uint8_t* duration_bytes = reinterpret_cast<const uint8_t*>(&duration_be);
    payload.insert(payload.end(), duration_bytes, duration_bytes + 4);
    
    // Priority timeout (4 bytes, 0 = infinite)
    uint32_t timeout = 0;
    uint32_t timeout_be = htonl(timeout);
    const uint8_t* timeout_bytes = reinterpret_cast<const uint8_t*>(&timeout_be);
    payload.insert(payload.end(), timeout_bytes, timeout_bytes + 4);
    
    // Origin (4 bytes, 0 = manual)
    uint32_t origin = 0;
    uint32_t origin_be = htonl(origin);
    const uint8_t* origin_bytes = reinterpret_cast<const uint8_t*>(&origin_be);
    payload.insert(payload.end(), origin_bytes, origin_bytes + 4);
    
    // LED count (4 bytes, big endian)
    uint32_t led_count = static_cast<uint32_t>(colors.size());
    uint32_t led_count_be = htonl(led_count);
    const uint8_t* count_bytes = reinterpret_cast<const uint8_t*>(&led_count_be);
    payload.insert(payload.end(), count_bytes, count_bytes + 4);
    
    // RGB data (3 bytes per LED)
    for (const auto& color : colors) {
        payload.push_back(color[0]);  // R
        payload.push_back(color[1]);  // G
        payload.push_back(color[2]);  // B
    }
    
    // Prepend size header (4 bytes, big endian)
    std::vector<uint8_t> message;
    uint32_t total_size = payload.size();
    uint32_t size_be = htonl(total_size);
    const uint8_t* size_bytes = reinterpret_cast<const uint8_t*>(&size_be);
    message.insert(message.end(), size_bytes, size_bytes + 4);
    message.insert(message.end(), payload.begin(), payload.end());
    
    return message;
}

} // namespace TVLED

