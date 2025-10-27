#pragma once

#include <opencv2/opencv.hpp>
#include <vector>
#include <string>
#include <memory>

namespace TVLED {

class HyperHDRClient {
public:
    HyperHDRClient(const std::string& host, int port, int priority = 100);
    ~HyperHDRClient();
    
    // Connect to HyperHDR server
    bool connect();
    
    // Disconnect from server
    void disconnect();
    
    // Send LED colors to HyperHDR
    // Colors should be in RGB format
    bool sendColors(const std::vector<cv::Vec3b>& colors);
    
    // Check if connected
    bool isConnected() const { return connected_; }
    
    // Set priority (lower = higher priority)
    void setPriority(int priority) { priority_ = priority; }
    
private:
    std::string host_;
    int port_;
    int priority_;
    bool connected_;
    int socket_fd_;
    
    // Helper methods
    bool sendMessage(const uint8_t* data, size_t size);
    std::string createJsonMessage(const std::vector<cv::Vec3b>& colors);
};

} // namespace TVLED

