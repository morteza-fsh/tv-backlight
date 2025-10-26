#include "core/LEDController.h"
#include "core/Config.h"
#include "utils/Logger.h"
#include <iostream>
#include <csignal>
#include <atomic>

using namespace TVLED;

std::atomic<bool> should_exit(false);
::TVLED::LEDController* g_controller = nullptr;

void signalHandler(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        LOG_INFO("Received signal, shutting down...");
        should_exit = true;
        if (g_controller) {
            g_controller->stop();
        }
    }
}

void printUsage(const char* program_name) {
    std::cout << "Usage: " << program_name << " [OPTIONS]\n\n"
              << "Options:\n"
              << "  --config <path>      Path to config file (default: config.json)\n"
              << "  --debug              Run in debug mode (static image)\n"
              << "  --live               Run in live mode (camera)\n"
              << "  --image <path>       Input image for debug mode\n"
              << "  --camera <device>    Camera device (default: /dev/video0)\n"
              << "  --single-frame       Process single frame and exit\n"
              << "  --save-debug         Save debug images\n"
              << "  --verbose            Enable verbose logging\n"
              << "  --help               Show this help message\n\n"
              << "Examples:\n"
              << "  " << program_name << " --debug --image test.png --single-frame --save-debug\n"
              << "  " << program_name << " --live --camera /dev/video0\n"
              << "  " << program_name << " --config my_config.json\n";
}

int main(int argc, char* argv[]) {
    // Parse command line arguments
    std::string config_path = "config.json";
    std::string mode;
    std::string image_path;
    std::string camera_device;
    bool single_frame = false;
    bool save_debug = false;
    bool verbose = false;
    
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        
        if (arg == "--help" || arg == "-h") {
            printUsage(argv[0]);
            return 0;
        } else if (arg == "--config" && i + 1 < argc) {
            config_path = argv[++i];
        } else if (arg == "--debug") {
            mode = "debug";
        } else if (arg == "--live") {
            mode = "live";
        } else if (arg == "--image" && i + 1 < argc) {
            image_path = argv[++i];
        } else if (arg == "--camera" && i + 1 < argc) {
            camera_device = argv[++i];
        } else if (arg == "--single-frame") {
            single_frame = true;
        } else if (arg == "--save-debug") {
            save_debug = true;
        } else if (arg == "--verbose" || arg == "-v") {
            verbose = true;
        } else {
            std::cerr << "Unknown option: " << arg << "\n";
            printUsage(argv[0]);
            return 1;
        }
    }
    
    // Setup logging
    if (verbose) {
        Logger::getInstance().setLevel(LogLevel::DEBUG);
    }
    
    LOG_INFO("=== LED Controller Starting ===");
    
    // Load configuration
    Config config;
    if (!config.loadFromFile(config_path)) {
        LOG_ERROR("Failed to load configuration from " + config_path);
        return 1;
    }
    
    // Override config with command line arguments
    if (!mode.empty()) {
        config.mode = mode;
        LOG_INFO("Mode overridden to: " + mode);
    }
    if (!image_path.empty()) {
        config.input_image = image_path;
        LOG_INFO("Input image overridden to: " + image_path);
    }
    if (!camera_device.empty()) {
        config.camera.device = camera_device;
        LOG_INFO("Camera device overridden to: " + camera_device);
    }
    
    // Create controller
    ::TVLED::LEDController controller(config);
    g_controller = &controller;
    
    // Setup signal handlers
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);
    
    // Initialize
    if (!controller.initialize()) {
        LOG_ERROR("Failed to initialize LED Controller");
        return 1;
    }
    
    // Run
    int result = 0;
    if (single_frame) {
        LOG_INFO("Processing single frame...");
        if (controller.processSingleFrame(save_debug)) {
            LOG_INFO("Single frame processed successfully");
            result = 0;
        } else {
            LOG_ERROR("Failed to process frame");
            result = 1;
        }
    } else {
        LOG_INFO("Starting continuous processing...");
        int frames = controller.run();
        if (frames > 0) {
            LOG_INFO("Processed " + std::to_string(frames) + " frames");
            result = 0;
        } else {
            LOG_ERROR("Processing failed");
            result = 1;
        }
    }
    
    LOG_INFO("=== LED Controller Stopped ===");
    return result;
}

