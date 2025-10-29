#pragma once

#include <opencv2/opencv.hpp>
#include <vector>
#include <string>
#include <memory>
#include <netinet/in.h>
#include <flatbuffers/flatbuffers.h>
#include "flatbuffer/hyperion_request_generated.h"

namespace TVLED {

class HyperHDRClient {
public:
    HyperHDRClient(const std::string& host, int port, int priority = 100, const std::string& origin = "cpp-tv-led");
    ~HyperHDRClient();
    
    // Connect to HyperHDR server via TCP
    bool connect();
    
    // Disconnect from server
    void disconnect();
    
    // Send LED colors to HyperHDR using FlatBuffers
    // Colors must be in RGB (R,G,B) 8-bit per channel.
    // If your source is OpenCV BGR, convert before calling.
    bool sendColors(const std::vector<cv::Vec3b>& colors);
    
    // Check if connected
    bool isConnected() const { return connected_; }
    
    // Set priority (lower = higher priority)
    void setPriority(int priority) { priority_ = priority; }
    
private:
    std::string host_;
    int port_;
    int priority_;
    std::string origin_;
    bool connected_;
    int socket_fd_;
    sockaddr_in server_addr_;
    
    // Helper methods
    bool sendTCPMessage(const uint8_t* data, size_t size);
    bool registerWithHyperHDR();
    std::vector<uint8_t> createFlatBufferMessage(const std::vector<cv::Vec3b>& colors);

    static bool sendAll(int fd, const uint8_t* data, size_t size);
};

} // namespace TVLED