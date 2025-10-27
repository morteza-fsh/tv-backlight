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
    
    // Create flatbuffer-style binary message for individual LED control
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
    // HyperHDR/Hyperion Flatbuffer protocol for individual LED colors
    // This is a simplified implementation that works with HyperHDR's flatbuffer server
    
    std::vector<uint8_t> data;
    
    // Build the raw LED color data (RGB triplets)
    for (const auto& color : colors) {
        data.push_back(color[0]);  // R
        data.push_back(color[1]);  // G
        data.push_back(color[2]);  // B
    }
    
    // Prepare the full message with header
    std::vector<uint8_t> message;
    
    // Message format for HyperHDR flatbuffer:
    // 4 bytes: message size (big endian)
    // 4 bytes: magic identifier "BHDR" (0x42484452)
    // 1 byte: message type (2 = color data)
    // 4 bytes: priority (big endian)
    // 4 bytes: duration in ms (0 = infinite) (big endian)
    // 4 bytes: LED count (big endian)
    // N*3 bytes: RGB data
    
    // Calculate payload size
    uint32_t payload_size = 1 + 4 + 4 + 4 + data.size(); // type + priority + duration + count + RGB data
    
    // Build the payload first
    std::vector<uint8_t> payload;
    
    // Message type (2 = LED colors)
    payload.push_back(0x02);
    
    // Priority (4 bytes, big endian)
    uint32_t priority_be = htonl(static_cast<uint32_t>(priority_));
    const uint8_t* priority_bytes = reinterpret_cast<const uint8_t*>(&priority_be);
    payload.insert(payload.end(), priority_bytes, priority_bytes + 4);
    
    // Duration (4 bytes, 0 = infinite, big endian)
    uint32_t duration = 0;
    uint32_t duration_be = htonl(duration);
    const uint8_t* duration_bytes = reinterpret_cast<const uint8_t*>(&duration_be);
    payload.insert(payload.end(), duration_bytes, duration_bytes + 4);
    
    // LED count (4 bytes, big endian)
    uint32_t led_count = static_cast<uint32_t>(colors.size());
    uint32_t led_count_be = htonl(led_count);
    const uint8_t* count_bytes = reinterpret_cast<const uint8_t*>(&led_count_be);
    payload.insert(payload.end(), count_bytes, count_bytes + 4);
    
    // RGB data
    payload.insert(payload.end(), data.begin(), data.end());
    
    // Now build the full message with size header and magic identifier
    
    // Size (4 bytes, big endian) - includes magic + payload
    uint32_t total_size = 4 + payload.size(); // magic (4 bytes) + payload
    uint32_t size_be = htonl(total_size);
    const uint8_t* size_bytes = reinterpret_cast<const uint8_t*>(&size_be);
    message.insert(message.end(), size_bytes, size_bytes + 4);
    
    // Magic identifier "BHDR" (0x42484452)
    message.push_back(0x42); // 'B'
    message.push_back(0x48); // 'H'
    message.push_back(0x44); // 'D'
    message.push_back(0x52); // 'R'
    
    // Payload
    message.insert(message.end(), payload.begin(), payload.end());
    
    LOG_INFO("Sending " + std::to_string(colors.size()) + " individual LED colors to HyperHDR");
    
    return message;
}

} // namespace TVLED

