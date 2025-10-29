#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <chrono>
#include <netinet/in.h> // sockaddr_in
#include <opencv2/core.hpp> // cv::Vec3b, cv::Vec3f

namespace TVLED {

class HyperHDRClient {
public:
    HyperHDRClient(const std::string& host, int port, int priority, const std::string& origin);
    ~HyperHDRClient();

    bool connect();
    void disconnect();

    // Send one LED frame (BGR 8-bit coming from OpenCV; converted to RGB for the wire/file)
    bool sendColors(const std::vector<cv::Vec3b>& colors);

    // Utilities for saving frames (for debugging, replay, etc.)
    // NDJSON: human-readable, one JSON object per line
    bool saveFrameAsNDJSON(const std::string& filePath,
                           std::chrono::system_clock::time_point ts,
                           double dt_ms,
                           const std::vector<cv::Vec3b>& colors,
                           bool append = true);

    // Binary: compact, header + RGB bytes
    bool saveFrameBinary(const std::string& filePath,
                         std::chrono::system_clock::time_point ts,
                         float dt_ms,
                         const std::vector<cv::Vec3b>& colors,
                         bool append = true);

    // If your pipeline is Vec3f in [0..1], convert to Vec3b 0..255 first:
    static std::vector<cv::Vec3b> to8bit(const std::vector<cv::Vec3f>& in);

private:
    bool sendTCPMessage(const uint8_t* data, size_t size);
    bool registerWithHyperHDR();
    std::vector<uint8_t> createFlatBufferMessage(const std::vector<cv::Vec3b>& colors);

private:
    std::string host_;
    int         port_;
    int         priority_;
    std::string origin_;

    bool connected_;
    int  socket_fd_;
    sockaddr_in server_addr_;
};

} // namespace TVLED