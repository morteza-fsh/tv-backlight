#pragma once

#include <string>
#include <iostream>
#include <mutex>
#include <sstream>
#include <chrono>
#include <iomanip>

namespace TVLED {

enum class LogLevel {
    DEBUG,
    INFO,
    WARN,
    ERROR
};

class Logger {
public:
    static Logger& getInstance() {
        static Logger instance;
        return instance;
    }

    void setLevel(LogLevel level) {
        std::lock_guard<std::mutex> lock(mutex_);
        level_ = level;
    }

    LogLevel getLevel() const {
        return level_;
    }

    void log(LogLevel level, const std::string& message) {
        if (level < level_) return;

        std::lock_guard<std::mutex> lock(mutex_);
        
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()) % 1000;
        
        std::stringstream ss;
        ss << "[" << std::put_time(std::localtime(&time), "%H:%M:%S")
           << "." << std::setfill('0') << std::setw(3) << ms.count() << "] ";
        
        switch(level) {
            case LogLevel::DEBUG: ss << "[DEBUG] "; break;
            case LogLevel::INFO:  ss << "[INFO ] "; break;
            case LogLevel::WARN:  ss << "[WARN ] "; break;
            case LogLevel::ERROR: ss << "[ERROR] "; break;
        }
        
        ss << message;
        
        if (level == LogLevel::ERROR) {
            std::cerr << ss.str() << std::endl;
        } else {
            std::cout << ss.str() << std::endl;
        }
    }

    void debug(const std::string& message) { log(LogLevel::DEBUG, message); }
    void info(const std::string& message) { log(LogLevel::INFO, message); }
    void warn(const std::string& message) { log(LogLevel::WARN, message); }
    void error(const std::string& message) { log(LogLevel::ERROR, message); }

private:
    Logger() : level_(LogLevel::INFO) {}
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    LogLevel level_;
    std::mutex mutex_;
};

// Convenience macros
#define LOG_DEBUG(msg) TVLED::Logger::getInstance().debug(msg)
#define LOG_INFO(msg) TVLED::Logger::getInstance().info(msg)
#define LOG_WARN(msg) TVLED::Logger::getInstance().warn(msg)
#define LOG_ERROR(msg) TVLED::Logger::getInstance().error(msg)

} // namespace TVLED

