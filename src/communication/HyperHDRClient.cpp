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

bool HyperHDRClient::sendColors(const std::vector<cv::Vec3b>& colors, const LEDLayout& layout) {
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
    auto message = createFlatBufferMessage(colors, layout);
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

std::vector<uint8_t> HyperHDRClient::createFlatBufferMessage(const std::vector<cv::Vec3b>& colors, const LEDLayout& layout) {
    // Colors are expected to be RGB (R,G,B).
    // If your source is OpenCV BGR, convert before calling this function.

    const int led_count = static_cast<int>(colors.size());
    LOG_INFO("Creating FlatBuffer message for " + std::to_string(led_count) + " LEDs");

    // Calculate reasonable image dimensions based on LED layout
    int image_width, image_height;
    
    if (layout.getFormat() == LEDLayoutFormat::GRID) {
        // For grid layout, use 10x the actual grid dimensions
        image_width = layout.getCols() * 10;
        image_height = layout.getRows() * 10;
    } else {
        // For HyperHDR layout, create a 10x larger 2D representation
        // Calculate total perimeter and create a square-ish image
        int top_count = layout.getTopCount();
        int bottom_count = layout.getBottomCount();
        int left_count = layout.getLeftCount();
        int right_count = layout.getRightCount();
        
        // Calculate approximate width and height based on LED counts
        // Use a reasonable aspect ratio and minimum dimensions (10x larger)
        int total_horizontal = std::max(top_count, bottom_count);
        int total_vertical = std::max(left_count, right_count);
        
        // Ensure minimum dimensions and reasonable aspect ratio (10x larger)
        image_width = std::max(total_horizontal * 10, 320);  // Minimum 320 pixels wide (10x 32)
        image_height = std::max(total_vertical * 10, 240);   // Minimum 240 pixels tall (10x 24)
        
        // Adjust to maintain reasonable aspect ratio (not too wide/tall)
        if (image_width > image_height * 3) {
            image_width = image_height * 3;
        }
        if (image_height > image_width * 3) {
            image_height = image_width * 3;
        }
        
        LOG_INFO("HyperHDR layout: T=" + std::to_string(top_count) + 
                " B=" + std::to_string(bottom_count) + 
                " L=" + std::to_string(left_count) + 
                " R=" + std::to_string(right_count) +
                " -> Image: " + std::to_string(image_width) + "x" + std::to_string(image_height) + " (10x larger)");
    }

    // Create 2D image data
    std::vector<uint8_t> rgb_data;
    rgb_data.reserve(static_cast<size_t>(image_width * image_height * 3));
    
    // Initialize with black background
    rgb_data.resize(static_cast<size_t>(image_width * image_height * 3), 0);
    
    // Map LED colors to appropriate positions in the 2D image
    if (layout.getFormat() == LEDLayoutFormat::GRID) {
        // For grid layout, map to 10x scaled grid positions
        for (int row = 0; row < layout.getRows(); row++) {
            for (int col = 0; col < layout.getCols(); col++) {
                int led_idx = layout.gridToLEDIndex(row, col);
                if (led_idx >= 0 && led_idx < led_count) {
                    const auto& color = colors[led_idx];
                    
                    // Create a 10x10 pixel block for each LED
                    int base_x = col * 10;
                    int base_y = row * 10;
                    
                    for (int dy = 0; dy < 10; dy++) {
                        for (int dx = 0; dx < 10; dx++) {
                            int x = base_x + dx;
                            int y = base_y + dy;
                            if (x < image_width && y < image_height) {
                                int pixel_idx = (y * image_width + x) * 3;
                                rgb_data[pixel_idx + 0] = color[0]; // R
                                rgb_data[pixel_idx + 1] = color[1]; // G
                                rgb_data[pixel_idx + 2] = color[2]; // B
                            }
                        }
                    }
                }
            }
        }
    } else {
        // For HyperHDR layout, map LEDs to edge positions as 10x10 adjacent squares
        // Direction: top-left → top-right → bottom-right → bottom-left
        int top_count = layout.getTopCount();
        int bottom_count = layout.getBottomCount();
        int left_count = layout.getLeftCount();
        int right_count = layout.getRightCount();
        
        int led_idx = 0;
        
        // Top edge (left to right) - 10x10 squares along the top
        for (int i = 0; i < top_count && led_idx < led_count; i++) {
            const auto& color = colors[led_idx];
            int x_start = i * 10;  // Each LED gets a 10-pixel wide square
            int y_start = 0;        // Top edge
            
            // Fill 10x10 square
            for (int dy = 0; dy < 10; dy++) {
                for (int dx = 0; dx < 10; dx++) {
                    int px = x_start + dx;
                    int py = y_start + dy;
                    if (px < image_width && py < image_height) {
                        int pixel_idx = (py * image_width + px) * 3;
                        rgb_data[pixel_idx + 0] = color[0]; // R
                        rgb_data[pixel_idx + 1] = color[1]; // G
                        rgb_data[pixel_idx + 2] = color[2]; // B
                    }
                }
            }
            led_idx++;
        }
        
        // Right edge (top to bottom) - 10x10 squares along the right
        for (int i = 0; i < right_count && led_idx < led_count; i++) {
            const auto& color = colors[led_idx];
            int x_start = image_width - 10;  // Right edge (10 pixels from right)
            int y_start = i * 10;             // Each LED gets a 10-pixel tall square
            
            // Fill 10x10 square
            for (int dy = 0; dy < 10; dy++) {
                for (int dx = 0; dx < 10; dx++) {
                    int px = x_start + dx;
                    int py = y_start + dy;
                    if (px >= 0 && px < image_width && py < image_height) {
                        int pixel_idx = (py * image_width + px) * 3;
                        rgb_data[pixel_idx + 0] = color[0]; // R
                        rgb_data[pixel_idx + 1] = color[1]; // G
                        rgb_data[pixel_idx + 2] = color[2]; // B
                    }
                }
            }
            led_idx++;
        }
        
        // Bottom edge (right to left) - 10x10 squares along the bottom
        for (int i = 0; i < bottom_count && led_idx < led_count; i++) {
            const auto& color = colors[led_idx];
            int x_start = image_width - 10 - (i * 10);  // Right to left
            int y_start = image_height - 10;             // Bottom edge
            
            // Fill 10x10 square
            for (int dy = 0; dy < 10; dy++) {
                for (int dx = 0; dx < 10; dx++) {
                    int px = x_start + dx;
                    int py = y_start + dy;
                    if (px >= 0 && px < image_width && py >= 0 && py < image_height) {
                        int pixel_idx = (py * image_width + px) * 3;
                        rgb_data[pixel_idx + 0] = color[0]; // R
                        rgb_data[pixel_idx + 1] = color[1]; // G
                        rgb_data[pixel_idx + 2] = color[2]; // B
                    }
                }
            }
            led_idx++;
        }
        
        // Left edge (bottom to top) - 10x10 squares along the left
        for (int i = 0; i < left_count && led_idx < led_count; i++) {
            const auto& color = colors[led_idx];
            int x_start = 0;                              // Left edge
            int y_start = image_height - 10 - (i * 10);  // Bottom to top
            
            // Fill 10x10 square
            for (int dy = 0; dy < 10; dy++) {
                for (int dx = 0; dx < 10; dx++) {
                    int px = x_start + dx;
                    int py = y_start + dy;
                    if (px >= 0 && px < image_width && py >= 0 && py < image_height) {
                        int pixel_idx = (py * image_width + px) * 3;
                        rgb_data[pixel_idx + 0] = color[0]; // R
                        rgb_data[pixel_idx + 1] = color[1]; // G
                        rgb_data[pixel_idx + 2] = color[2]; // B
                    }
                }
            }
            led_idx++;
        }
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
        ", image=" + std::to_string(image_width) + "x" + std::to_string(image_height) +
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