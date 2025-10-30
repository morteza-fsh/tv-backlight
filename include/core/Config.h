#pragma once

#include <string>
#include <nlohmann/json.hpp>

namespace TVLED {

struct CameraConfig {
    std::string device = "/dev/video0";
    int width = 1640;   // IMX219 2x2 binned full frame (no crop)
    int height = 1232;  // Full sensor with binning
    int fps = 41;       // Max FPS at binned resolution
    int sensor_mode = -1;  // -1 = auto, or specify mode number (use rpicam-hello --list-cameras to see modes)
    
    // Scaling configuration
    bool enable_scaling = true;
    int scaled_width = 820;   // Scale down 2x for performance
    int scaled_height = 616;  // Maintains aspect ratio
};

struct HyperHDRConfig {
    bool enabled = false;
    std::string host = "127.0.0.1";
    int port = 19400;
    int priority = 100;
    bool use_linear_format = false;  // true = 1-pixel tall linear format, false = layout-based 2D format
    bool use_udp = false;  // true = UDP RAW mode (simple RGB bytes), false = TCP FlatBuffers mode
    int udp_port = 19446;  // UDP port for RAW mode (default Hyperion UDP port)
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
    float offset_x = 0.0f;
    float offset_y = 0.0f;
};

} // namespace TVLED

