#pragma once

#include <string>
#include <nlohmann/json.hpp>

namespace TVLED {

struct CameraConfig {
    std::string device = "/dev/video0";
    int width = 3840;   // 4K width
    int height = 2160;  // 4K height
    int fps = 30;
    
    // Scaling configuration
    bool enable_scaling = true;
    int scaled_width = 960;   // Quarter of full width (540p for faster processing)
    int scaled_height = 540;  // Quarter of full height
};

struct HyperHDRConfig {
    bool enabled = false;
    std::string host = "127.0.0.1";
    int port = 19400;
    int priority = 100;
};

struct LEDLayoutConfig {
    std::string format = "grid";  // "grid" or "hyperhdr"
    
    // Grid format
    int grid_rows = 5;
    int grid_cols = 8;
    
    // HyperHDR format (edge counts)
    int hyperhdr_top = 20;
    int hyperhdr_bottom = 20;
    int hyperhdr_left = 10;
    int hyperhdr_right = 10;
};

struct BezierConfig {
    std::string left_bezier;
    std::string bottom_bezier;
    std::string right_bezier;
    std::string top_bezier;
    
    bool use_direct_bezier_curves = true;
    int bezier_samples = 50;
    int polygon_samples = 15;
};

struct PerformanceConfig {
    int target_fps = 0;  // 0 = max speed
    bool enable_parallel_processing = true;
    int parallel_chunk_size = 4;
};

struct VisualizationConfig {
    int grid_cell_width = 60;
    int grid_cell_height = 40;
    int debug_boundary_thickness = 3;
    int debug_corner_radius = 10;
};

struct ColorSettingsConfig {
    bool show_coordinates = true;
    float coordinate_font_scale = 0.4f;
    int border_thickness = 1;
};

struct ColorExtractionConfig {
    std::string mode = "edge_slices";  // "grid" or "edge_slices"
    float horizontal_coverage_percent = 20.0f;  // 0-100
    float vertical_coverage_percent = 20.0f;    // 0-100
    int horizontal_slices = 10;  // Number of horizontal strips for top/bottom edges
    int vertical_slices = 8;     // Number of vertical strips for left/right edges
};

class Config {
public:
    Config() = default;
    
    // Load configuration from JSON file
    bool loadFromFile(const std::string& filename);
    
    // Save configuration to JSON file
    bool saveToFile(const std::string& filename) const;
    
    // Validate configuration
    bool validate() const;
    
    // Mode
    std::string mode = "debug";  // "debug" or "live"
    
    // Input/Output
    std::string input_image = "img2.png";
    std::string output_directory = "output";
    
    // Sub-configurations
    CameraConfig camera;
    HyperHDRConfig hyperhdr;
    LEDLayoutConfig led_layout;
    BezierConfig bezier;
    PerformanceConfig performance;
    VisualizationConfig visualization;
    ColorSettingsConfig color_settings;
    ColorExtractionConfig color_extraction;
    
    // Scaling
    float scale_factor = 2.0f;
};

} // namespace TVLED

