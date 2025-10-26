#pragma once

#include <chrono>
#include <string>
#include <sstream>
#include "Logger.h"

namespace TVLED {

class PerformanceTimer {
public:
    explicit PerformanceTimer(const std::string& name, bool autoReport = true)
        : name_(name), autoReport_(autoReport), running_(false) {
        start();
    }

    ~PerformanceTimer() {
        if (autoReport_ && running_) {
            stop();
            report();
        }
    }

    void start() {
        start_ = std::chrono::high_resolution_clock::now();
        running_ = true;
    }

    void stop() {
        end_ = std::chrono::high_resolution_clock::now();
        running_ = false;
    }

    long long elapsedMilliseconds() const {
        auto endTime = running_ ? std::chrono::high_resolution_clock::now() : end_;
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            endTime - start_).count();
    }

    long long elapsedMicroseconds() const {
        auto endTime = running_ ? std::chrono::high_resolution_clock::now() : end_;
        return std::chrono::duration_cast<std::chrono::microseconds>(
            endTime - start_).count();
    }

    void report() const {
        std::stringstream ss;
        ss << name_ << ": " << elapsedMilliseconds() << " ms";
        LOG_INFO(ss.str());
    }

    void reportDetailed() const {
        std::stringstream ss;
        ss << name_ << ": " << elapsedMilliseconds() << " ms (" 
           << elapsedMicroseconds() << " μs)";
        LOG_INFO(ss.str());
    }

private:
    std::string name_;
    bool autoReport_;
    bool running_;
    std::chrono::high_resolution_clock::time_point start_;
    std::chrono::high_resolution_clock::time_point end_;
};

} // namespace TVLED

