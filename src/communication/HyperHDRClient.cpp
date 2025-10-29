#include "communication/HyperHDRClient.h"
#include "utils/Logger.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <sstream>
#include <vector>
#include <cstdint>

// If needed for OpenCV Vec3b
#include <opencv2/core/types.hpp>

namespace TVLED {

// ---- helper: reliably send all bytes (send can return partial writes) ----
static bool sendAll(int fd, const uint8_t* data, size_t size) {
    size_t total = 0;
    while (total < size) {
        ssize_t sent = ::send(fd, data + total, size - total, 0);
        if (sent <= 0) {
            return false; // error or connection closed
        }
        total += static_cast<size_t>(sent);
    }
    return true;
}

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
    socket_fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd_ < 0) {
        LOG_ERROR("Failed to create TCP socket");
        return false;
    }

    // Setup server address
    server_addr_.sin_family = AF_INET;
    server_addr_.sin_port = htons(port_);

    if (::inet_pton(AF_INET, host_.c_str(), &server_addr_.sin_addr) <= 0) {
        LOG_ERROR("Invalid address: " + host_);
        ::close(socket_fd_);
        socket_fd_ = -1;
        return false;
    }

    // Connect to server
    if (::connect(socket_fd_, reinterpret_cast<struct sockaddr*>(&server_addr_), sizeof(server_addr_)) < 0) {
        LOG_ERROR("Failed to connect to HyperHDR server at " + host_ + ":" + std::to_string(port_));
        ::close(socket_fd_);
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
        ::close(socket_fd_);
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
    if (message.empty()) {
        LOG_ERROR("FlatBuffer message creation failed");
        return false;
    }

    // Send message via TCP
    return sendTCPMessage(message.data(), message.size());
}

bool HyperHDRClient::sendTCPMessage(const uint8_t* data, size_t size) {
    if (socket_fd_ < 0) {
        LOG_ERROR("Socket not open");
        return false;
    }

    // HyperHDR/Hyperion FlatBuffers server expects:
    //  [ 4-byte length prefix in BIG-ENDIAN (network byte order) ] + [ payload ]
    uint32_t len = static_cast<uint32_t>(size);
    uint32_t be_len = htonl(len); // ---- FIX: convert to network byte order ----

    LOG_INFO("Sending TCP message: payload size = " + std::to_string(size) + " bytes");

    // Send big-endian length prefix
    if (!sendAll(socket_fd_, reinterpret_cast<const uint8_t*>(&be_len), sizeof(be_len))) {
        LOG_ERROR("Failed to send length prefix");
        return false;
    }
    LOG_DEBUG("Length prefix (big-endian) sent successfully");

    // Send payload
    if (!sendAll(socket_fd_, data, size)) {
        LOG_ERROR("Failed to send FlatBuffer payload");
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

    // Build a 1xN RGB image matching the LED strip
    int image_width  = led_count; // Each LED is one pixel in width
    int image_height = 1;         // Single row

    // Create RGB image data (R, G, B — we convert from OpenCV BGR)
    std::vector<uint8_t> rgb_data;
    rgb_data.reserve(static_cast<size_t>(image_width * image_height * 3));

    for (const auto& color : colors) {
        // OpenCV Vec3b is B,G,R -> convert to R,G,B
        rgb_data.push_back(color[2]); // R
        rgb_data.push_back(color[1]); // G
        rgb_data.push_back(color[0]); // B
    }

    LOG_INFO("RGB data size: " + std::to_string(rgb_data.size()) + " bytes");
    LOG_INFO("Image dimensions: " + std::to_string(image_width) + "x" + std::to_string(image_height));

    // Verify size
    size_t expected_size = static_cast<size_t>(image_width * image_height * 3);
    if (rgb_data.size() != expected_size) {
        LOG_ERROR("RGB data size mismatch: expected " + std::to_string(expected_size) +
                  ", got " + std::to_string(rgb_data.size()));
        return {};
    }

    // Build FlatBuffer
    flatbuffers::FlatBufferBuilder fbb(1024 + rgb_data.size());

    auto img_data  = fbb.CreateVector(rgb_data);

    // NOTE:
    // If your generated schema has fields for pixel format and stride/lineLength,
    // prefer that overload and set:
    //   pixelFormat = RGB888
    //   lineLength  = image_width * 3
    // For your current headers (as in your code), we keep the 3-arg RawImage.
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