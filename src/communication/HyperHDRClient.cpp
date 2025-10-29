#include "communication/HyperHDRClient.h"
#include "utils/Logger.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#include <cstring>
#include <sstream>

namespace TVLED {

using namespace hyperionnet;

namespace {
// Build a compact preview of the first N RGB pixels: [r,g,b] [r,g,b] ...
static std::string buildRgbPreview(const std::vector<uint8_t>& rgbBytes, size_t maxPixels) {
    const size_t totalPixels = rgbBytes.size() / 3;
    const size_t previewPixels = std::min(maxPixels, totalPixels);
    std::ostringstream oss;
    for (size_t i = 0; i < previewPixels; ++i) {
        const size_t base = i * 3;
        const int r = static_cast<int>(rgbBytes[base + 0]);
        const int g = static_cast<int>(rgbBytes[base + 1]);
        const int b = static_cast<int>(rgbBytes[base + 2]);
        if (i > 0) oss << ' ';
        oss << '[' << r << ',' << g << ',' << b << ']';
    }
    if (previewPixels < totalPixels) {
        oss << " ...";
    }
    return oss.str();
}
}

// ---- helper: reliably send all bytes (send can return partial writes) ----
bool HyperHDRClient::sendAll(int fd, const uint8_t* data, size_t size) {
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
    server_addr_.sin_port = htons(static_cast<uint16_t>(port_));
    
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
    
    // Log input colors before sending
    std::ostringstream oss;
    oss << "sendColors() received " << colors.size() << " colors, first few: ";
    for (size_t i = 0; i < std::min(size_t(8), colors.size()); ++i) {
        const auto& c = colors[i];
        oss << "[" << static_cast<int>(c[0]) << "," 
            << static_cast<int>(c[1]) << "," 
            << static_cast<int>(c[2]) << "] ";
    }
    LOG_INFO(oss.str());
    
    LOG_WARN("⚠️  IMPORTANT: Ensure HyperHDR LED layout config matches the " + 
             std::to_string(colors.size()) + " LEDs being sent!");
    
    // Build FlatBuffer message for LED colors
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
        LOG_ERROR("Socket is not open");
        return false;
    }

    // HyperHDR/Hyperion FlatBuffers server expects:
    //   4-byte length prefix in BIG-ENDIAN (network byte order) + payload
    uint32_t len = static_cast<uint32_t>(size);
    uint32_t be_len = htonl(len); // host → network (big-endian)

    LOG_INFO("Sending TCP message: payload size = " + std::to_string(size) + " bytes");

    // Send length prefix (big-endian)
    if (!sendAll(socket_fd_, reinterpret_cast<const uint8_t*>(&be_len), sizeof(be_len))) {
        LOG_ERROR("Failed to send length prefix");
        return false;
    }
    LOG_DEBUG("Length prefix (big-endian) sent successfully");

    // Send payload (handle partial writes)
    if (!sendAll(socket_fd_, data, size)) {
        LOG_ERROR("Failed to send FlatBuffer payload");
        return false;
    }

    LOG_INFO("FlatBuffer payload sent successfully");
    return true;
}

bool HyperHDRClient::registerWithHyperHDR() {
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
    // Colors are expected to be RGB (R,G,B).
    // If your source is OpenCV BGR, convert before calling this function.

    const int led_count    = static_cast<int>(colors.size());
    const int image_width  = led_count; // 1 pixel per LED horizontally
    const int image_height = 1;         // single row

    LOG_INFO("Creating FlatBuffer message for " + std::to_string(led_count) + " LEDs");

    // Pack RGB bytes
    std::vector<uint8_t> rgb_data;
    rgb_data.reserve(static_cast<size_t>(image_width * image_height * 3));
    
    for (const auto& c : colors) {
        // c = (R, G, B)
        rgb_data.push_back(c[0]); // R
        rgb_data.push_back(c[1]); // G
        rgb_data.push_back(c[2]); // B
    }
    
    const size_t expected_size = static_cast<size_t>(image_width * image_height * 3);
    if (rgb_data.size() != expected_size) {
        LOG_ERROR("RGB data size mismatch: expected " + std::to_string(expected_size) + 
                  ", got " + std::to_string(rgb_data.size()));
        return {};
    }

    // Log a brief summary of the RGB payload to verify correctness
    uint64_t checksum = 0;
    for (uint8_t v : rgb_data) checksum += static_cast<uint64_t>(v);
    const std::string preview = buildRgbPreview(rgb_data, 12 /*pixels*/);
    LOG_INFO(
        "RGB payload: leds=" + std::to_string(led_count) +
        ", bytes=" + std::to_string(rgb_data.size()) +
        ", checksum=" + std::to_string(checksum) +
        ", preview=" + preview
    );
    
    // Dump first 48 raw bytes in hex for debugging
    std::ostringstream hex_dump;
    hex_dump << "First 48 bytes (hex): ";
    for (size_t i = 0; i < std::min(size_t(48), rgb_data.size()); ++i) {
        char buf[4];
        snprintf(buf, sizeof(buf), "%02X ", rgb_data[i]);
        hex_dump << buf;
    }
    LOG_DEBUG(hex_dump.str());

    flatbuffers::FlatBufferBuilder fbb(1024 + rgb_data.size());
    
    // Create RawImage (schema variant without pixelFormat/stride)
    auto img_data  = fbb.CreateVector(rgb_data);
    auto raw_image = CreateRawImage(fbb, img_data, image_width, image_height);
    
    // Create Image with explicit duration field set to -1 (infinite)
    auto image = CreateImage(fbb, ImageType_RawImage, raw_image.Union(), -1);
    
    // Wrap into a Request(Command_Image)
    RequestBuilder req_builder(fbb);
    req_builder.add_command_type(Command_Image);
    req_builder.add_command(image.Union());
    auto request = req_builder.Finish();
    
    // Finish with no file identifier (Hyperion doesn't use one)
    fbb.Finish(request);
    
    const uint8_t* buf = fbb.GetBufferPointer();
    const size_t len = fbb.GetSize();
    
    LOG_INFO("Created FlatBuffer message with " + std::to_string(colors.size()) + " LED colors (" + std::to_string(len) + " bytes)");
    
    // Log FlatBuffer structure info
    LOG_DEBUG("FlatBuffer details: width=" + std::to_string(image_width) + 
              ", height=" + std::to_string(image_height) + 
              ", image_type=RawImage, command=Image, duration=-1");
    
    // Dump the first 64 bytes of the FlatBuffer message for debugging
    std::ostringstream fb_hex;
    fb_hex << "FlatBuffer first 64 bytes (hex): ";
    for (size_t i = 0; i < std::min(size_t(64), len); ++i) {
        char hex_buf[4];
        snprintf(hex_buf, sizeof(hex_buf), "%02X ", buf[i]);
        fb_hex << hex_buf;
    }
    LOG_DEBUG(fb_hex.str());
    
    return std::vector<uint8_t>(buf, buf + len);
}

} // namespace TVLED