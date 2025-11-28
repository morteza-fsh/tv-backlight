#pragma once

#include <opencv2/opencv.hpp>
#include <vector>
#include <string>
#include <memory>

namespace TVLED {

/**
 * USBController - Direct USB serial communication for LED control
 * 
 * This class provides direct RGB data transmission to a USB-connected
 * LED controller (e.g., Arduino, ESP32) via serial port.
 * 
 * Protocol format:
 * - Header: 0xFF 0xFF 0xAA (3 bytes)
 * - LED count: 2 bytes (big-endian, max 65535 LEDs)
 * - RGB data: N * 3 bytes (R, G, B for each LED)
 * - Checksum: 1 byte (XOR of all RGB data bytes)
 * 
 * Total packet size: 6 + (LED_COUNT * 3) bytes
 */
class USBController {
public:
    /**
     * Constructor
     * @param device Serial device path (e.g., "/dev/ttyUSB0", "/dev/ttyACM0")
     * @param baudrate Baud rate (default: 115200)
     */
    USBController(const std::string& device, int baudrate = 115200);
    ~USBController();
    
    /**
     * Open and configure the serial port
     * @return true if successful, false otherwise
     */
    bool connect();
    
    /**
     * Close the serial port
     */
    void disconnect();
    
    /**
     * Send RGB color data directly to USB device
     * Colors must be in RGB format (R,G,B) 8-bit per channel.
     * If your source is OpenCV BGR, convert before calling.
     * 
     * @param colors Vector of RGB colors (one per LED)
     * @return true if data sent successfully, false otherwise
     */
    bool sendColors(const std::vector<cv::Vec3b>& colors);
    
    /**
     * Check if connected to USB device
     * @return true if connected, false otherwise
     */
    bool isConnected() const { return connected_; }
    
    /**
     * Get the device path
     * @return Device path string
     */
    std::string getDevice() const { return device_; }
    
    /**
     * Get the baud rate
     * @return Baud rate
     */
    int getBaudrate() const { return baudrate_; }

private:
    std::string device_;
    int baudrate_;
    bool connected_;
    int fd_;  // File descriptor for serial port
    
    /**
     * Create protocol packet from RGB data
     * @param colors RGB color data
     * @return Packet as byte vector
     */
    std::vector<uint8_t> createPacket(const std::vector<cv::Vec3b>& colors);
    
    /**
     * Calculate checksum (XOR of all data bytes)
     * @param data Data to checksum
     * @return Checksum byte
     */
    uint8_t calculateChecksum(const std::vector<uint8_t>& data);
    
    /**
     * Write data to serial port
     * @param data Data buffer
     * @param size Size of data
     * @return true if successful, false otherwise
     */
    bool writeData(const uint8_t* data, size_t size);
};

} // namespace TVLED

