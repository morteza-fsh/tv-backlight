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
    
#ifdef ENABLE_FLATBUFFERS
    // Create FlatBuffer message for LED colors
    auto message = createFlatBufferMessage(colors);
    
    // Send message via TCP
    return sendTCPMessage(message.data(), message.size());
#else
    LOG_WARN("FlatBuffers support not enabled. Cannot send colors to HyperHDR.");
    return false;
#endif
}

bool HyperHDRClient::sendTCPMessage(const uint8_t* data, size_t size) {
    // HyperHDR FlatBuffers server expects a length-prefix (uint32 LE) followed by the buffer
    uint32_t len = static_cast<uint32_t>(size);
    uint32_t le_len = len; // Convert to little endian
    
    // Convert to little endian manually for portability
    uint8_t len_bytes[4];
    len_bytes[0] = len & 0xFF;
    len_bytes[1] = (len >> 8) & 0xFF;
    len_bytes[2] = (len >> 16) & 0xFF;
    len_bytes[3] = (len >> 24) & 0xFF;
    
    // Send length prefix
    ssize_t sent = send(socket_fd_, len_bytes, sizeof(len_bytes), 0);
    if (sent != sizeof(len_bytes)) {
        LOG_ERROR("Failed to send length prefix");
        return false;
    }
    
    // Send payload
    sent = send(socket_fd_, data, size, 0);
    if (sent != static_cast<ssize_t>(size)) {
        LOG_ERROR("Failed to send FlatBuffer payload");
        return false;
    }
    
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
#ifdef ENABLE_FLATBUFFERS
    using namespace hyperionnet;
    
    // Convert colors to RGB byte array
    std::vector<uint8_t> rgb_data;
    rgb_data.reserve(colors.size() * 3);
    
    for (const auto& color : colors) {
        rgb_data.push_back(color[2]);  // B
        rgb_data.push_back(color[1]);  // G  
        rgb_data.push_back(color[0]);  // R
    }
    
    // Build FlatBuffer
    flatbuffers::FlatBufferBuilder fbb(1024 + rgb_data.size());
    
    // Create RawImage with RGB data
    auto img_data = fbb.CreateVector(rgb_data);
    auto raw_image = CreateRawImage(fbb, img_data, colors.size(), 1);
    
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
#else
    // Return empty vector if FlatBuffers not enabled
    return std::vector<uint8_t>();
#endif
}

} // namespace TVLED

