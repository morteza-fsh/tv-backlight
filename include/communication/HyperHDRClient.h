#pragma once

#include <opencv2/opencv.hpp>
#include <vector>
#include <string>
#include <memory>
#include <netinet/in.h>
#include <flatbuffers/flatbuffers.h>
#include "flatbuffer/hyperion_request_generated.h"
#include "communication/LEDLayout.h"

namespace TVLED {

class HyperHDRClient {
public:
    HyperHDRClient(const std::string& host, int port, int priority = 100, const std::string& origin = "cpp-tv-led", bool use_udp = false, int udp_port = 19446);
    ~HyperHDRClient();
    
    // Connect to HyperHDR server via TCP or UDP
    bool connect();
    
    // Disconnect from server
    void disconnect();
    
    // Send LED colors to HyperHDR using FlatBuffers (TCP) or UDP RAW (UDP)
    // Colors must be in RGB (R,G,B) 8-bit per channel.
    // If your source is OpenCV BGR, convert before calling.
    // LED layout is used to create proper 2D image structure (TCP mode only).
    bool sendColors(const std::vector<cv::Vec3b>& colors, const LEDLayout& layout);
    
    // Send LED colors to HyperHDR using linear format (1 pixel tall, width = LED count)
    // Each pixel in the single row represents one LED's color directly.
    // Colors must be in RGB (R,G,B) 8-bit per channel.
    // This is the standard way to send per-LED color frames using FlatBuffers (TCP) or UDP RAW (UDP).
    bool sendColorsLinear(const std::vector<cv::Vec3b>& colors);
    
    // Check if connected
    bool isConnected() const { return connected_; }
    
    // Set priority (lower = higher priority) - only used in TCP mode
    void setPriority(int priority) { priority_ = priority; }
    
private:
    std::string host_;
    int port_;
    int priority_;
    std::string origin_;
    bool use_udp_;
    int udp_port_;
    bool connected_;
    int socket_fd_;
    sockaddr_in server_addr_;
    
    // Helper methods
    bool sendTCPMessage(const uint8_t* data, size_t size);
    bool sendUDPMessage(const uint8_t* data, size_t size);
    bool registerWithHyperHDR();
    std::vector<uint8_t> createFlatBufferMessage(const std::vector<cv::Vec3b>& colors, const LEDLayout& layout);
    std::vector<uint8_t> createFlatBufferMessageLinear(const std::vector<cv::Vec3b>& colors);

    static bool sendAll(int fd, const uint8_t* data, size_t size);
};

} // namespace TVLED