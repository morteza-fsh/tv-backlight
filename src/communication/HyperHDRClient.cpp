#include "communication/HyperHDRClient.h"
#include "utils/Logger.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <sstream>

namespace TVLED {

HyperHDRClient::HyperHDRClient(const std::string& host, int port, int priority, const std::string& origin)
    : host_(host), port_(port), priority_(priority), origin_(origin), connected_(false), socket_fd_(-1) {
    std::memset(&server_addr_, 0, sizeof(server_addr_));
}

HyperHDRClient::~HyperHDRClient() {
    disconnect();
}

bool HyperHDRClient::connect() {
    if (connected_) {
        LOG_WARN("Already connected to HyperHDR");
        return true;
    }
    
    // Create TCP socket
    socket_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd_ < 0) {
        LOG_ERROR("Failed to create TCP socket");
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
    
    // Connect to server
    if (::connect(socket_fd_, (struct sockaddr*)&server_addr_, sizeof(server_addr_)) < 0) {
        LOG_ERROR("Failed to connect to HyperHDR server at " + host_ + ":" + std::to_string(port_));
        close(socket_fd_);
        socket_fd_ = -1;
        return false;
    }
    
    connected_ = true;
    LOG_INFO("TCP connection established to HyperHDR at " + host_ + ":" + std::to_string(port_));
    
    // Register with HyperHDR
    if (!registerWithHyperHDR()) {
        LOG_ERROR("Failed to register with HyperHDR");
        disconnect();
        return false;
    }
    
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
    
    // Create FlatBuffer message for LED colors
    auto message = createFlatBufferMessage(colors);
    
    // Send message via TCP
    return sendTCPMessage(message.data(), message.size());
}

bool HyperHDRClient::sendTCPMessage(const uint8_t* data, size_t size) {
    // HyperHDR FlatBuffers server expects a length-prefix (uint32 LE) followed by the buffer
    uint32_t len = static_cast<uint32_t>(size);
    uint32_t le_len = len; // Convert to little endian
    
    LOG_INFO("Sending TCP message: payload size = " + std::to_string(size) + " bytes");
    
    // Convert to little endian manually for portability
    uint8_t len_bytes[4];
    len_bytes[0] = len & 0xFF;
    len_bytes[1] = (len >> 8) & 0xFF;
    len_bytes[2] = (len >> 16) & 0xFF;
    len_bytes[3] = (len >> 24) & 0xFF;
    
    LOG_DEBUG("Length prefix bytes: " + std::to_string(len_bytes[0]) + " " + 
              std::to_string(len_bytes[1]) + " " + 
              std::to_string(len_bytes[2]) + " " + 
              std::to_string(len_bytes[3]));
    
    // Send length prefix
    ssize_t sent = send(socket_fd_, len_bytes, sizeof(len_bytes), 0);
    if (sent != sizeof(len_bytes)) {
        LOG_ERROR("Failed to send length prefix: sent " + std::to_string(sent) + " bytes");
        return false;
    }
    
    LOG_INFO("Length prefix sent successfully");
    
    // Send payload
    sent = send(socket_fd_, data, size, 0);
    if (sent != static_cast<ssize_t>(size)) {
        LOG_ERROR("Failed to send FlatBuffer payload: sent " + std::to_string(sent) + " of " + std::to_string(size) + " bytes");
        return false;
    }
    
    LOG_INFO("FlatBuffer payload sent successfully");
    return true;
}

bool HyperHDRClient::registerWithHyperHDR() {
    using namespace hyperionnet;
    
    flatbuffers::FlatBufferBuilder fbb(1024);
    
    // Create origin string
    auto origin_str = fbb.CreateString(origin_);
    
    // Create Register command
    auto register_cmd = CreateRegister(fbb, origin_str, priority_);
    
    // Create Request with Register command
    RequestBuilder req_builder(fbb);
    req_builder.add_command_type(Command_Register);
    req_builder.add_command(register_cmd.Union());
    auto request = req_builder.Finish();
    
    fbb.Finish(request);
    
    // Send registration message
    const uint8_t* buf = fbb.GetBufferPointer();
    const size_t len = fbb.GetSize();
    
    if (!sendTCPMessage(buf, len)) {
        LOG_ERROR("Failed to send registration message");
        return false;
    }
    
    LOG_INFO("Successfully registered with HyperHDR as '" + origin_ + "' with priority " + std::to_string(priority_));
    return true;
}

std::vector<uint8_t> HyperHDRClient::createFlatBufferMessage(const std::vector<cv::Vec3b>& colors) {
    using namespace hyperionnet;
    
    // Debug: Log the actual LED count and data size
    int led_count = static_cast<int>(colors.size());
    LOG_INFO("Creating FlatBuffer message for " + std::to_string(led_count) + " LEDs");
    
    // Try a different approach: instead of using RawImage, let's try using
    // the Color command for LED data. The Color command might be more appropriate
    // for setting LED colors rather than sending image data
    
    // However, the Color command is for a single color, not multiple LEDs
    // So we need to find a different approach
    
    // Let me try a different approach: create a RawImage that represents
    // the LED layout in a way that HyperHDR can understand
    
    // The issue might be that HyperHDR is expecting the RawImage to represent
    // actual image dimensions that match the LED layout configuration
    // Let's try creating a RawImage that matches the LED layout configuration
    
    // For LED strips, we typically have a single row of LEDs
    // But we need to be careful about the dimensions to avoid the frame size issue
    
    // Try a more conservative approach: create a small 2D image
    // that represents the LED layout in a way HyperHDR can understand
    
    int image_width = led_count;   // Each LED gets one pixel width
    int image_height = 1;          // Single row of LEDs
    
    // Create RGB image data (3 bytes per pixel)
    std::vector<uint8_t> rgb_data;
    rgb_data.reserve(image_width * image_height * 3);
    
    for (const auto& color : colors) {
        rgb_data.push_back(color[2]);  // B
        rgb_data.push_back(color[1]);  // G  
        rgb_data.push_back(color[0]);  // R
    }
    
    LOG_INFO("RGB data size: " + std::to_string(rgb_data.size()) + " bytes");
    LOG_INFO("Image dimensions: " + std::to_string(image_width) + "x" + std::to_string(image_height));
    
    // Verify the data size is reasonable
    size_t expected_size = image_width * image_height * 3;
    if (rgb_data.size() != expected_size) {
        LOG_ERROR("RGB data size mismatch: expected " + std::to_string(expected_size) + 
                  ", got " + std::to_string(rgb_data.size()));
        return std::vector<uint8_t>();
    }
    
    // Build FlatBuffer
    flatbuffers::FlatBufferBuilder fbb(1024 + rgb_data.size());
    
    // Create RawImage with proper dimensions
    auto img_data = fbb.CreateVector(rgb_data);
    auto raw_image = CreateRawImage(fbb, img_data, image_width, image_height);
    
    // Create Image with RawImage
    auto image = CreateImage(fbb, ImageType_RawImage, raw_image.Union());
    
    // Create Request with Image command
    RequestBuilder req_builder(fbb);
    req_builder.add_command_type(Command_Image);
    req_builder.add_command(image.Union());
    auto request = req_builder.Finish();
    
    fbb.Finish(request);
    
    // Get the buffer
    const uint8_t* buf = fbb.GetBufferPointer();
    const size_t len = fbb.GetSize();
    
    LOG_INFO("Created FlatBuffer message with " + std::to_string(colors.size()) + " LED colors (" + std::to_string(len) + " bytes)");
    
    return std::vector<uint8_t>(buf, buf + len);
}

} // namespace TVLED

