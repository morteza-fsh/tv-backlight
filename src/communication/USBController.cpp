#include "communication/USBController.h"
#include "utils/Logger.h"

#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <cstring>
#include <sstream>
#include <iomanip>

namespace TVLED {

namespace {
    // Protocol constants
    constexpr uint8_t HEADER_BYTE_1 = 0xFF;
    constexpr uint8_t HEADER_BYTE_2 = 0xFF;
    constexpr uint8_t HEADER_BYTE_3 = 0xAA;
    
    /**
     * Convert baud rate integer to termios speed constant
     */
    speed_t getBaudRate(int baudrate) {
        switch (baudrate) {
            case 9600:   return B9600;
            case 19200:  return B19200;
            case 38400:  return B38400;
            case 57600:  return B57600;
            case 115200: return B115200;
            case 230400: return B230400;
#ifdef B460800
            case 460800: return B460800;
#endif
#ifdef B500000
            case 500000: return B500000;
#endif
#ifdef B576000
            case 576000: return B576000;
#endif
#ifdef B921600
            case 921600: return B921600;
#endif
#ifdef B1000000
            case 1000000: return B1000000;
#endif
#ifdef B1152000
            case 1152000: return B1152000;
#endif
#ifdef B1500000
            case 1500000: return B1500000;
#endif
#ifdef B2000000
            case 2000000: return B2000000;
#endif
#ifdef B2500000
            case 2500000: return B2500000;
#endif
#ifdef B3000000
            case 3000000: return B3000000;
#endif
#ifdef B3500000
            case 3500000: return B3500000;
#endif
#ifdef B4000000
            case 4000000: return B4000000;
#endif
            default:
                LOG_WARN("Unsupported baud rate " + std::to_string(baudrate) + 
                         ", defaulting to 115200");
                return B115200;
        }
    }
    
    /**
     * Format bytes as hex string for debugging
     */
    std::string formatHex(const uint8_t* data, size_t size, size_t maxBytes = 16) {
        std::ostringstream oss;
        size_t displaySize = std::min(size, maxBytes);
        for (size_t i = 0; i < displaySize; ++i) {
            oss << std::hex << std::setfill('0') << std::setw(2) 
                << static_cast<int>(data[i]) << " ";
        }
        if (size > maxBytes) {
            oss << "... (" << size << " bytes total)";
        }
        return oss.str();
    }
}

USBController::USBController(const std::string& device, int baudrate)
    : device_(device), baudrate_(baudrate), connected_(false), fd_(-1) {
}

USBController::~USBController() {
    disconnect();
}

bool USBController::connect() {
    if (connected_) {
        LOG_WARN("Already connected to USB device");
        return true;
    }
    
    LOG_INFO("Opening USB serial device: " + device_);
    
    // Open serial port with read/write access, no controlling terminal
    fd_ = ::open(device_.c_str(), O_RDWR | O_NOCTTY | O_SYNC);
    if (fd_ < 0) {
        LOG_ERROR("Failed to open device " + device_ + ": " + std::string(std::strerror(errno)));
        return false;
    }
    
    // Get current serial port settings
    struct termios tty;
    if (::tcgetattr(fd_, &tty) != 0) {
        LOG_ERROR("Failed to get serial port attributes: " + std::string(std::strerror(errno)));
        ::close(fd_);
        fd_ = -1;
        return false;
    }
    
    // Configure serial port
    speed_t speed = getBaudRate(baudrate_);
    ::cfsetospeed(&tty, speed);  // Output baud rate
    ::cfsetispeed(&tty, speed);  // Input baud rate
    
    // 8N1 mode (8 data bits, no parity, 1 stop bit)
    tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS8;  // 8-bit chars
    tty.c_cflag &= ~(PARENB | PARODD);           // No parity
    tty.c_cflag &= ~CSTOPB;                       // 1 stop bit
    tty.c_cflag &= ~CRTSCTS;                      // No hardware flow control
    tty.c_cflag |= (CLOCAL | CREAD);              // Ignore modem controls, enable reading
    
    // Non-canonical mode (raw mode)
    tty.c_lflag = 0;
    
    // No input processing
    tty.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);
    
    // No output processing
    tty.c_oflag = 0;
    
    // Blocking read with 1 second timeout
    tty.c_cc[VMIN]  = 0;   // Non-blocking read
    tty.c_cc[VTIME] = 10;  // 1 second timeout (in deciseconds)
    
    // Apply settings
    if (::tcsetattr(fd_, TCSANOW, &tty) != 0) {
        LOG_ERROR("Failed to set serial port attributes: " + std::string(std::strerror(errno)));
        ::close(fd_);
        fd_ = -1;
        return false;
    }
    
    // Flush any existing data
    ::tcflush(fd_, TCIOFLUSH);
    
    connected_ = true;
    LOG_INFO("USB serial device opened successfully: " + device_ + 
             " @ " + std::to_string(baudrate_) + " baud");
    
    return true;
}

void USBController::disconnect() {
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
    connected_ = false;
    LOG_INFO("Disconnected from USB device");
}

bool USBController::sendColors(const std::vector<cv::Vec3b>& colors) {
    if (!connected_) {
        LOG_ERROR("Not connected to USB device");
        return false;
    }
    
    if (colors.empty()) {
        LOG_WARN("No colors to send");
        return false;
    }
    
    if (colors.size() > 65535) {
        LOG_ERROR("Too many LEDs: " + std::to_string(colors.size()) + 
                  " (max 65535)");
        return false;
    }
    
    // Log first few colors for debugging
    std::ostringstream oss;
    oss << "Sending " << colors.size() << " RGB colors to USB, first few: ";
    for (size_t i = 0; i < std::min(size_t(5), colors.size()); ++i) {
        const auto& c = colors[i];
        oss << "[" << static_cast<int>(c[0]) << "," 
            << static_cast<int>(c[1]) << "," 
            << static_cast<int>(c[2]) << "] ";
    }
    LOG_DEBUG(oss.str());
    
    // Create packet
    auto packet = createPacket(colors);
    
    LOG_DEBUG("Packet size: " + std::to_string(packet.size()) + " bytes, " +
              "header: " + formatHex(packet.data(), 16));
    
    // Send packet
    if (!writeData(packet.data(), packet.size())) {
        LOG_ERROR("Failed to send data to USB device");
        return false;
    }
    
    LOG_INFO("Successfully sent " + std::to_string(colors.size()) + 
             " LED colors (" + std::to_string(packet.size()) + " bytes)");
    
    return true;
}

std::vector<uint8_t> USBController::createPacket(const std::vector<cv::Vec3b>& colors) {
    const uint16_t led_count = static_cast<uint16_t>(colors.size());
    const size_t rgb_data_size = led_count * 3;
    
    // Packet structure:
    // [HEADER(3)] [LED_COUNT(2)] [RGB_DATA(N*3)] [CHECKSUM(1)]
    std::vector<uint8_t> packet;
    packet.reserve(6 + rgb_data_size);
    
    // Header (3 bytes)
    packet.push_back(HEADER_BYTE_1);
    packet.push_back(HEADER_BYTE_2);
    packet.push_back(HEADER_BYTE_3);
    
    // LED count (2 bytes, big-endian)
    packet.push_back(static_cast<uint8_t>(led_count >> 8));    // High byte
    packet.push_back(static_cast<uint8_t>(led_count & 0xFF));  // Low byte
    
    // RGB data (N * 3 bytes)
    std::vector<uint8_t> rgb_data;
    rgb_data.reserve(rgb_data_size);
    
    for (const auto& color : colors) {
        rgb_data.push_back(color[0]);  // R
        rgb_data.push_back(color[1]);  // G
        rgb_data.push_back(color[2]);  // B
    }
    
    // Calculate checksum before appending data
    uint8_t checksum = calculateChecksum(rgb_data);
    
    // Append RGB data to packet
    packet.insert(packet.end(), rgb_data.begin(), rgb_data.end());
    
    // Checksum (1 byte)
    packet.push_back(checksum);
    
    return packet;
}

uint8_t USBController::calculateChecksum(const std::vector<uint8_t>& data) {
    uint8_t checksum = 0;
    for (uint8_t byte : data) {
        checksum ^= byte;
    }
    return checksum;
}

bool USBController::writeData(const uint8_t* data, size_t size) {
    if (fd_ < 0) {
        LOG_ERROR("Serial port not open");
        return false;
    }
    
    size_t total_written = 0;
    while (total_written < size) {
        ssize_t written = ::write(fd_, data + total_written, size - total_written);
        if (written < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                // Retry on temporary error
                usleep(1000);  // Wait 1ms
                continue;
            }
            LOG_ERROR("Failed to write to serial port: " + std::string(std::strerror(errno)));
            return false;
        }
        total_written += written;
    }
    
    // Wait for data to be transmitted
    ::tcdrain(fd_);
    
    return true;
}

} // namespace TVLED

