#include "communication/HyperHDRClient.h"
#include "utils/Logger.h"

// POSIX
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

// STL
#include <cstring>
#include <sstream>
#include <fstream>
#include <iomanip>
#include <algorithm>

// OpenCV
#include <opencv2/core.hpp>

// FlatBuffers (generated from Hyperion/HyperHDR schemas)
// Make sure your include paths point to your generated headers.
// Namespaces/types below assume "hyperionnet" like your original code.
#include "hyperion_request_generated.h"
// #include "hyperion_reply_generated.h" // If you later parse replies

// Cross-platform gmtime
#if defined(_WIN32)
  #define GMTIME_S(tm_ptr, timep) gmtime_s((tm_ptr), (timep))
#else
  #define GMTIME_S(tm_ptr, timep) gmtime_r((timep), (tm_ptr))
#endif

namespace {

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

// Clamp to 0..255
static inline uint8_t u8(int v) {
    return static_cast<uint8_t>(std::max(0, std::min(255, v)));
}

// RFC3339 with milliseconds (UTC, trailing Z)
static std::string iso8601_ms(std::chrono::system_clock::time_point tp) {
    using namespace std::chrono;
    auto t  = system_clock::to_time_t(tp);
    auto ms = duration_cast<milliseconds>(tp.time_since_epoch()) % 1000;

    std::tm tm{};
    GMTIME_S(&tm, &t);

    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%S") << '.'
        << std::setw(3) << std::setfill('0') << ms.count()
        << "Z";
    return oss.str();
}

#pragma pack(push, 1)
struct LedFileHeader {
    uint32_t magic;      // 0x4C454446 'L' 'E' 'D' 'F' (little-endian in file)
    uint16_t version;    // 1
    uint16_t led_count;
    uint64_t ts_ms;      // epoch ms
    float    dt_ms;      // frame delta in ms
};
#pragma pack(pop)

} // anonymous

namespace TVLED {

using namespace hyperionnet;

HyperHDRClient::HyperHDRClient(const std::string& host, int port, int priority, const std::string& origin)
    : host_(host), port_(port), priority_(priority), origin_(origin),
      connected_(false), socket_fd_(-1) {
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
    server_addr_.sin_port   = htons(static_cast<uint16_t>(port_));

    if (::inet_pton(AF_INET, host_.c_str(), &server_addr_.sin_addr) <= 0) {
        LOG_ERROR("Invalid address: " + host_);
        ::close(socket_fd_);
        socket_fd_ = -1;
        return false;
    }

    // Connect to server
    if (::connect(socket_fd_, reinterpret_cast<struct sockaddr*>(&server_addr_),
                  sizeof(server_addr_)) < 0) {
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

    // Send message via TCP (with big-endian length prefix)
    return sendTCPMessage(message.data(), message.size());
}

bool HyperHDRClient::sendTCPMessage(const uint8_t* data, size_t size) {
    if (socket_fd_ < 0) {
        LOG_ERROR("Socket not open");
        return false;
    }

    // Required framing: [4-byte length prefix BIG-ENDIAN] + [payload]
    uint32_t len   = static_cast<uint32_t>(size);
    uint32_t beLen = htonl(len); // host → network (big-endian)

    LOG_INFO("Sending TCP message: payload size = " + std::to_string(size) + " bytes");

    // Send big-endian length prefix
    if (!sendAll(socket_fd_, reinterpret_cast<const uint8_t*>(&beLen), sizeof(beLen))) {
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
    const size_t   len = fbb.GetSize();

    if (!sendTCPMessage(buf, len)) {
        LOG_ERROR("Failed to send registration message");
        return false;
    }

    LOG_INFO("Registered with HyperHDR as '" + origin_ + "' with priority " + std::to_string(priority_));
    return true;
}

std::vector<uint8_t> HyperHDRClient::createFlatBufferMessage(const std::vector<cv::Vec3b>& colors) {
    // Build a 1xN RGB image matching the LED strip.
    // NOTE: We convert from OpenCV BGR -> RGB explicitly.

    int image_width  = static_cast<int>(colors.size());
    int image_height = 1;

    std::vector<uint8_t> rgb_data;
    rgb_data.reserve(static_cast<size_t>(image_width * image_height * 3));

    for (const auto& c : colors) {
        // OpenCV Vec3b is B, G, R → convert to R, G, B
        rgb_data.push_back(c[2]); // R
        rgb_data.push_back(c[1]); // G
        rgb_data.push_back(c[0]); // B
    }

    size_t expected_size = static_cast<size_t>(image_width * image_height * 3);
    if (rgb_data.size() != expected_size) {
        LOG_ERROR("RGB data size mismatch: expected " + std::to_string(expected_size) +
                  ", got " + std::to_string(rgb_data.size()));
        return {};
    }

    flatbuffers::FlatBufferBuilder fbb(1024 + rgb_data.size());

    auto img_data = fbb.CreateVector(rgb_data);

    // IMPORTANT:
    // If your generated schema supports pixel format and stride/lineLength, prefer that overload:
    //   auto raw_image = CreateRawImage(fbb, img_data, image_width, image_height,
    //                                   PixelFormat_RGB888, image_width * 3);
    // For the 3-argument schema (as in your original), use:
    auto raw_image = CreateRawImage(fbb, img_data, image_width, image_height);

    // Build Image wrapper (and include priority if your schema supports it)
    ImageBuilder ib(fbb);
    ib.add_type(ImageType_RawImage);
    ib.add_image(raw_image.Union());
    // If your Image table has priority: ib.add_priority(priority_);
    auto image = ib.Finish();

    RequestBuilder req_builder(fbb);
    req_builder.add_command_type(Command_Image);
    req_builder.add_command(image.Union());
    auto request = req_builder.Finish();

    fbb.Finish(request);

    const uint8_t* buf = fbb.GetBufferPointer();
    const size_t   len = fbb.GetSize();

    LOG_INFO("Created FlatBuffer message for " + std::to_string(colors.size()) +
             " LEDs (" + std::to_string(len) + " bytes)");

    return std::vector<uint8_t>(buf, buf + len);
}

// ----- Saving helpers -------------------------------------------------------

bool HyperHDRClient::saveFrameAsNDJSON(const std::string& filePath,
                                       std::chrono::system_clock::time_point ts,
                                       double dt_ms,
                                       const std::vector<cv::Vec3b>& colors,
                                       bool append) {
    std::ofstream out(filePath, append ? std::ios::app : std::ios::trunc);
    if (!out) {
        LOG_ERROR("Failed to open file for NDJSON: " + filePath);
        return false;
    }

    out << '{'
        << "\"ts\":\"" << iso8601_ms(ts) << "\","
        << "\"dt_ms\":" << std::fixed << std::setprecision(3) << dt_ms << ','
        << "\"format\":\"RGB\",\"led_count\":" << colors.size() << ",\"leds\":[";

    for (size_t i = 0; i < colors.size(); ++i) {
        const auto& c = colors[i]; // B,G,R
        int R = c[2], G = c[1], B = c[0];
        out << '[' << u8(R) << ',' << u8(G) << ',' << u8(B) << ']';
        if (i + 1 < colors.size()) out << ',';
    }
    out << "]}\n";
    return true;
}

bool HyperHDRClient::saveFrameBinary(const std::string& filePath,
                                     std::chrono::system_clock::time_point ts,
                                     float dt_ms,
                                     const std::vector<cv::Vec3b>& colors,
                                     bool append) {
    std::ofstream out(filePath, (append ? std::ios::app : std::ios::trunc) | std::ios::binary);
    if (!out) {
        LOG_ERROR("Failed to open file for binary: " + filePath);
        return false;
    }

    using namespace std::chrono;
    auto ms = duration_cast<milliseconds>(ts.time_since_epoch()).count();

    LedFileHeader h{};
    h.magic     = 0x4C454446u; // 'LEDF'
    h.version   = 1;
    h.led_count = static_cast<uint16_t>(std::min<size_t>(colors.size(), 0xFFFF));
    h.ts_ms     = static_cast<uint64_t>(ms);
    h.dt_ms     = dt_ms;

    out.write(reinterpret_cast<const char*>(&h), sizeof(h));

    // Write RGB bytes
    for (const auto& c : colors) {
        uint8_t R = u8(c[2]), G = u8(c[1]), B = u8(c[0]); // BGR -> RGB
        out.put(static_cast<char>(R));
        out.put(static_cast<char>(G));
        out.put(static_cast<char>(B));
    }
    return true;
}

std::vector<cv::Vec3b> HyperHDRClient::to8bit(const std::vector<cv::Vec3f>& in) {
    std::vector<cv::Vec3b> out;
    out.reserve(in.size());
    for (const auto& c : in) {
        // c = (B, G, R) in [0..1]
        out.emplace_back(
            u8(static_cast<int>(std::lround(c[0] * 255.0f))), // B
            u8(static_cast<int>(std::lround(c[1] * 255.0f))), // G
            u8(static_cast<int>(std::lround(c[2] * 255.0f)))  // R
        );
    }
    return out;
}

} // namespace TVLED