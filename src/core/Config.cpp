#include "core/Config.h"
#include "utils/Logger.h"
#include <fstream>
#include <sstream>

using json = nlohmann::json;

namespace TVLED {

bool Config::loadFromFile(const std::string& filename) {
    try {
        std::ifstream file(filename);
        if (!file.is_open()) {
            LOG_ERROR("Could not open config file: " + filename);
            return false;
        }
        
        LOG_INFO("Loading configuration from " + filename);
        
        json j;
        file >> j;
        file.close();
        
        // Parse mode
        mode = j.value("mode", "debug");
        
        // Parse basic settings
        input_image = j.value("input_image", "img2.png");
        output_directory = j.value("output_directory", "output");
        
        // Parse camera settings
        if (j.contains("camera")) {
            auto cam = j["camera"];
            camera.device = cam.value("device", "/dev/video0");
            camera.width = cam.value("width", 1640);
            camera.height = cam.value("height", 1232);
            camera.fps = cam.value("fps", 41);
            camera.sensor_mode = cam.value("sensor_mode", -1);
            
            // Parse scaling settings
            camera.enable_scaling = cam.value("enable_scaling", true);
            camera.scaled_width = cam.value("scaled_width", 820);
            camera.scaled_height = cam.value("scaled_height", 616);
        }
        
        // Parse HyperHDR settings
        if (j.contains("hyperhdr")) {
            auto hdr = j["hyperhdr"];
            hyperhdr.enabled = hdr.value("enabled", false);
            hyperhdr.host = hdr.value("host", "127.0.0.1");
            hyperhdr.port = hdr.value("port", 19400);
            hyperhdr.priority = hdr.value("priority", 100);
        }
        
        // Parse LED layout
        if (j.contains("led_layout")) {
            auto layout = j["led_layout"];
            led_layout.format = layout.value("format", "grid");
            
            if (layout.contains("grid")) {
                auto grid = layout["grid"];
                led_layout.grid_rows = grid.value("rows", 5);
                led_layout.grid_cols = grid.value("cols", 8);
            }
            
            if (layout.contains("hyperhdr")) {
                auto hdr_layout = layout["hyperhdr"];
                led_layout.hyperhdr_top = hdr_layout.value("top", 20);
                led_layout.hyperhdr_bottom = hdr_layout.value("bottom", 20);
                led_layout.hyperhdr_left = hdr_layout.value("left", 10);
                led_layout.hyperhdr_right = hdr_layout.value("right", 10);
            }
        } else if (j.contains("grid")) {
            // Backward compatibility with old config format
            auto grid = j["grid"];
            led_layout.grid_rows = grid.value("rows", 5);
            led_layout.grid_cols = grid.value("cols", 8);
        }
        
        // Parse bezier curves
        if (j.contains("bezier_curves")) {
            auto bezier_curves = j["bezier_curves"];
            bezier.left_bezier = bezier_curves.value("left_bezier", "");
            bezier.bottom_bezier = bezier_curves.value("bottom_bezier", "");
            bezier.right_bezier = bezier_curves.value("right_bezier", "");
            bezier.top_bezier = bezier_curves.value("top_bezier", "");
        }
        
        // Parse bezier settings
        if (j.contains("bezier_settings")) {
            auto bezier_settings = j["bezier_settings"];
            bezier.use_direct_bezier_curves = bezier_settings.value("use_direct_bezier_curves", true);
            bezier.bezier_samples = bezier_settings.value("bezier_samples", 50);
            bezier.polygon_samples = bezier_settings.value("polygon_samples", 15);
        }
        
        // Parse scaling
        if (j.contains("scaling")) {
            auto scaling = j["scaling"];
            scale_factor = scaling.value("scale_factor", 2.0f);
            offset_x = scaling.value("offset_x", 0.0f);
            offset_y = scaling.value("offset_y", 0.0f);
        }
        
        // Parse visualization settings
        if (j.contains("visualization")) {
            auto viz = j["visualization"];
            visualization.grid_cell_width = viz.value("grid_cell_width", 60);
            visualization.grid_cell_height = viz.value("grid_cell_height", 40);
            visualization.debug_boundary_thickness = viz.value("debug_boundary_thickness", 3);
            visualization.debug_corner_radius = viz.value("debug_corner_radius", 10);
        }
        
        // Parse color settings
        if (j.contains("color_settings")) {
            auto color = j["color_settings"];
            color_settings.show_coordinates = color.value("show_coordinates", true);
            color_settings.coordinate_font_scale = color.value("coordinate_font_scale", 0.4f);
            color_settings.border_thickness = color.value("border_thickness", 1);
        }
        
        // Parse performance settings
        if (j.contains("performance")) {
            auto perf = j["performance"];
            performance.target_fps = perf.value("target_fps", 0);
            performance.enable_parallel_processing = perf.value("enable_parallel_processing", true);
            performance.parallel_chunk_size = perf.value("parallel_chunk_size", 4);
        }
        
        // Parse color extraction settings
        if (j.contains("color_extraction")) {
            auto ce = j["color_extraction"];
            color_extraction.mode = ce.value("mode", "edge_slices");
            color_extraction.horizontal_coverage_percent = ce.value("horizontal_coverage_percent", 20.0f);
            color_extraction.vertical_coverage_percent = ce.value("vertical_coverage_percent", 20.0f);
            color_extraction.horizontal_slices = ce.value("horizontal_slices", 10);
            color_extraction.vertical_slices = ce.value("vertical_slices", 8);
        }
        
        LOG_INFO("Configuration loaded successfully");
        return true;
        
    } catch (const json::exception& e) {
        LOG_ERROR(std::string("Error parsing JSON config: ") + e.what());
        return false;
    } catch (const std::exception& e) {
        LOG_ERROR(std::string("Error loading config: ") + e.what());
        return false;
    }
}

bool Config::saveToFile(const std::string& filename) const {
    try {
        json j;
        
        j["mode"] = mode;
        j["input_image"] = input_image;
        j["output_directory"] = output_directory;
        
        j["camera"]["device"] = camera.device;
        j["camera"]["width"] = camera.width;
        j["camera"]["height"] = camera.height;
        j["camera"]["fps"] = camera.fps;
        j["camera"]["sensor_mode"] = camera.sensor_mode;
        j["camera"]["enable_scaling"] = camera.enable_scaling;
        j["camera"]["scaled_width"] = camera.scaled_width;
        j["camera"]["scaled_height"] = camera.scaled_height;
        
        j["hyperhdr"]["enabled"] = hyperhdr.enabled;
        j["hyperhdr"]["host"] = hyperhdr.host;
        j["hyperhdr"]["port"] = hyperhdr.port;
        j["hyperhdr"]["priority"] = hyperhdr.priority;
        
        j["led_layout"]["format"] = led_layout.format;
        j["led_layout"]["grid"]["rows"] = led_layout.grid_rows;
        j["led_layout"]["grid"]["cols"] = led_layout.grid_cols;
        j["led_layout"]["hyperhdr"]["top"] = led_layout.hyperhdr_top;
        j["led_layout"]["hyperhdr"]["bottom"] = led_layout.hyperhdr_bottom;
        j["led_layout"]["hyperhdr"]["left"] = led_layout.hyperhdr_left;
        j["led_layout"]["hyperhdr"]["right"] = led_layout.hyperhdr_right;
        
        j["bezier_curves"]["left_bezier"] = bezier.left_bezier;
        j["bezier_curves"]["bottom_bezier"] = bezier.bottom_bezier;
        j["bezier_curves"]["right_bezier"] = bezier.right_bezier;
        j["bezier_curves"]["top_bezier"] = bezier.top_bezier;
        
        j["bezier_settings"]["use_direct_bezier_curves"] = bezier.use_direct_bezier_curves;
        j["bezier_settings"]["bezier_samples"] = bezier.bezier_samples;
        j["bezier_settings"]["polygon_samples"] = bezier.polygon_samples;
        
        j["scaling"]["scale_factor"] = scale_factor;
        
        j["visualization"]["grid_cell_width"] = visualization.grid_cell_width;
        j["visualization"]["grid_cell_height"] = visualization.grid_cell_height;
        j["visualization"]["debug_boundary_thickness"] = visualization.debug_boundary_thickness;
        j["visualization"]["debug_corner_radius"] = visualization.debug_corner_radius;
        
        j["color_settings"]["show_coordinates"] = color_settings.show_coordinates;
        j["color_settings"]["coordinate_font_scale"] = color_settings.coordinate_font_scale;
        j["color_settings"]["border_thickness"] = color_settings.border_thickness;
        
        j["performance"]["target_fps"] = performance.target_fps;
        j["performance"]["enable_parallel_processing"] = performance.enable_parallel_processing;
        j["performance"]["parallel_chunk_size"] = performance.parallel_chunk_size;
        
        j["color_extraction"]["mode"] = color_extraction.mode;
        j["color_extraction"]["horizontal_coverage_percent"] = color_extraction.horizontal_coverage_percent;
        j["color_extraction"]["vertical_coverage_percent"] = color_extraction.vertical_coverage_percent;
        j["color_extraction"]["horizontal_slices"] = color_extraction.horizontal_slices;
        j["color_extraction"]["vertical_slices"] = color_extraction.vertical_slices;
        
        std::ofstream file(filename);
        if (!file.is_open()) {
            LOG_ERROR("Could not open file for writing: " + filename);
            return false;
        }
        
        file << j.dump(2);
        file.close();
        
        LOG_INFO("Configuration saved to " + filename);
        return true;
        
    } catch (const std::exception& e) {
        LOG_ERROR(std::string("Error saving config: ") + e.what());
        return false;
    }
}

bool Config::validate() const {
    bool valid = true;
    
    if (mode != "debug" && mode != "live") {
        LOG_ERROR("Invalid mode: " + mode + " (must be 'debug' or 'live')");
        valid = false;
    }
    
    if (mode == "debug" && input_image.empty()) {
        LOG_ERROR("Debug mode requires input_image to be specified");
        valid = false;
    }
    
    if (bezier.left_bezier.empty() || bezier.bottom_bezier.empty() ||
        bezier.right_bezier.empty() || bezier.top_bezier.empty()) {
        LOG_ERROR("All four bezier curves must be specified");
        valid = false;
    }
    
    if (led_layout.format != "grid" && led_layout.format != "hyperhdr") {
        LOG_ERROR("Invalid LED layout format: " + led_layout.format);
        valid = false;
    }
    
    if (led_layout.format == "grid") {
        if (led_layout.grid_rows <= 0 || led_layout.grid_cols <= 0) {
            LOG_ERROR("Grid rows and cols must be positive");
            valid = false;
        }
    }
    
    if (color_extraction.mode != "grid" && color_extraction.mode != "edge_slices") {
        LOG_ERROR("Invalid color extraction mode: " + color_extraction.mode);
        valid = false;
    }
    
    if (color_extraction.horizontal_coverage_percent < 0 || 
        color_extraction.horizontal_coverage_percent > 100) {
        LOG_ERROR("Horizontal coverage percent must be between 0 and 100");
        valid = false;
    }
    
    if (color_extraction.vertical_coverage_percent < 0 || 
        color_extraction.vertical_coverage_percent > 100) {
        LOG_ERROR("Vertical coverage percent must be between 0 and 100");
        valid = false;
    }
    
    if (color_extraction.horizontal_slices <= 0 || color_extraction.vertical_slices <= 0) {
        LOG_ERROR("Horizontal and vertical slices must be positive");
        valid = false;
    }
    
    return valid;
}

} // namespace TVLED

