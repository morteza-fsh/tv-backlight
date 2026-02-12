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
            
            // Parse autofocus settings
            camera.autofocus_mode = cam.value("autofocus_mode", "default");
            camera.lens_position = cam.value("lens_position", 0.0f);
            
            // Parse white balance settings
            camera.awb_mode = cam.value("awb_mode", "auto");
            camera.awb_gain_red = cam.value("awb_gain_red", 0.0f);
            camera.awb_gain_blue = cam.value("awb_gain_blue", 0.0f);
            camera.awb_temperature = cam.value("awb_temperature", 0.0f);
            
            // Parse gain settings
            camera.analogue_gain = cam.value("analogue_gain", 0.0f);
            camera.digital_gain = cam.value("digital_gain", 0.0f);
            
            // Parse exposure settings
            camera.exposure_time = cam.value("exposure_time", 0);
            
            // Parse color correction matrix
            if (cam.contains("color_correction_matrix") && cam["color_correction_matrix"].is_array()) {
                camera.color_correction_matrix.clear();
                for (const auto& val : cam["color_correction_matrix"]) {
                    if (val.is_number()) {
                        camera.color_correction_matrix.push_back(val.get<float>());
                    }
                }
                // Validate: should be 9 values for 3x3 matrix
                if (camera.color_correction_matrix.size() != 9) {
                    LOG_WARN("Color correction matrix should have 9 values (3x3), got " + 
                             std::to_string(camera.color_correction_matrix.size()));
                    camera.color_correction_matrix.clear();
                }
            }
            
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
            hyperhdr.use_linear_format = hdr.value("use_linear_format", false);
        }
        
        // Parse USB settings
        if (j.contains("usb")) {
            auto usb_cfg = j["usb"];
            usb.enabled = usb_cfg.value("enabled", false);
            usb.device = usb_cfg.value("device", "/dev/ttyUSB0");
            usb.baudrate = usb_cfg.value("baudrate", 115200);
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
            flip_horizontal = scaling.value("flip_horizontal", false);
            flip_vertical = scaling.value("flip_vertical", false);
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
            color_extraction.method = ce.value("method", "dominant");
            color_extraction.horizontal_coverage_percent = ce.value("horizontal_coverage_percent", 20.0f);
            color_extraction.vertical_coverage_percent = ce.value("vertical_coverage_percent", 20.0f);
            color_extraction.horizontal_slices = ce.value("horizontal_slices", 10);
            color_extraction.vertical_slices = ce.value("vertical_slices", 8);
        }
        
        // Parse gamma correction settings
        if (j.contains("gamma_correction")) {
            auto gc = j["gamma_correction"];
            gamma_correction.enabled = gc.value("enabled", true);
            
            // Check if using 8-point gamma format (4 corners + 4 edge centers)
            if (gc.contains("top_center") && gc.contains("right_center") && 
                gc.contains("bottom_center") && gc.contains("left_center")) {
                // New 8-point format
                auto tl = gc["top_left"];
                gamma_correction.top_left.gamma_red = tl.value("gamma_red", 2.2);
                gamma_correction.top_left.gamma_green = tl.value("gamma_green", 2.2);
                gamma_correction.top_left.gamma_blue = tl.value("gamma_blue", 2.2);
                
                auto tc = gc["top_center"];
                gamma_correction.top_center.gamma_red = tc.value("gamma_red", 2.2);
                gamma_correction.top_center.gamma_green = tc.value("gamma_green", 2.2);
                gamma_correction.top_center.gamma_blue = tc.value("gamma_blue", 2.2);
                
                auto tr = gc["top_right"];
                gamma_correction.top_right.gamma_red = tr.value("gamma_red", 2.2);
                gamma_correction.top_right.gamma_green = tr.value("gamma_green", 2.2);
                gamma_correction.top_right.gamma_blue = tr.value("gamma_blue", 2.2);
                
                auto rc = gc["right_center"];
                gamma_correction.right_center.gamma_red = rc.value("gamma_red", 2.2);
                gamma_correction.right_center.gamma_green = rc.value("gamma_green", 2.2);
                gamma_correction.right_center.gamma_blue = rc.value("gamma_blue", 2.2);
                
                auto br = gc["bottom_right"];
                gamma_correction.bottom_right.gamma_red = br.value("gamma_red", 2.2);
                gamma_correction.bottom_right.gamma_green = br.value("gamma_green", 2.2);
                gamma_correction.bottom_right.gamma_blue = br.value("gamma_blue", 2.2);
                
                auto bc = gc["bottom_center"];
                gamma_correction.bottom_center.gamma_red = bc.value("gamma_red", 2.2);
                gamma_correction.bottom_center.gamma_green = bc.value("gamma_green", 2.2);
                gamma_correction.bottom_center.gamma_blue = bc.value("gamma_blue", 2.2);
                
                auto bl = gc["bottom_left"];
                gamma_correction.bottom_left.gamma_red = bl.value("gamma_red", 2.2);
                gamma_correction.bottom_left.gamma_green = bl.value("gamma_green", 2.2);
                gamma_correction.bottom_left.gamma_blue = bl.value("gamma_blue", 2.2);
                
                auto lc = gc["left_center"];
                gamma_correction.left_center.gamma_red = lc.value("gamma_red", 2.2);
                gamma_correction.left_center.gamma_green = lc.value("gamma_green", 2.2);
                gamma_correction.left_center.gamma_blue = lc.value("gamma_blue", 2.2);
            }
            // Check if using 4-corner format
            else if (gc.contains("top_left") && gc.contains("top_right") && 
                     gc.contains("bottom_left") && gc.contains("bottom_right")) {
                // 4-corner format - calculate edge centers as averages
                auto tl = gc["top_left"];
                gamma_correction.top_left.gamma_red = tl.value("gamma_red", 2.2);
                gamma_correction.top_left.gamma_green = tl.value("gamma_green", 2.2);
                gamma_correction.top_left.gamma_blue = tl.value("gamma_blue", 2.2);
                
                auto tr = gc["top_right"];
                gamma_correction.top_right.gamma_red = tr.value("gamma_red", 2.2);
                gamma_correction.top_right.gamma_green = tr.value("gamma_green", 2.2);
                gamma_correction.top_right.gamma_blue = tr.value("gamma_blue", 2.2);
                
                auto bl = gc["bottom_left"];
                gamma_correction.bottom_left.gamma_red = bl.value("gamma_red", 2.2);
                gamma_correction.bottom_left.gamma_green = bl.value("gamma_green", 2.2);
                gamma_correction.bottom_left.gamma_blue = bl.value("gamma_blue", 2.2);
                
                auto br = gc["bottom_right"];
                gamma_correction.bottom_right.gamma_red = br.value("gamma_red", 2.2);
                gamma_correction.bottom_right.gamma_green = br.value("gamma_green", 2.2);
                gamma_correction.bottom_right.gamma_blue = br.value("gamma_blue", 2.2);
                
                // Calculate edge centers as average of adjacent corners
                gamma_correction.top_center.gamma_red = (gamma_correction.top_left.gamma_red + gamma_correction.top_right.gamma_red) / 2.0;
                gamma_correction.top_center.gamma_green = (gamma_correction.top_left.gamma_green + gamma_correction.top_right.gamma_green) / 2.0;
                gamma_correction.top_center.gamma_blue = (gamma_correction.top_left.gamma_blue + gamma_correction.top_right.gamma_blue) / 2.0;
                
                gamma_correction.right_center.gamma_red = (gamma_correction.top_right.gamma_red + gamma_correction.bottom_right.gamma_red) / 2.0;
                gamma_correction.right_center.gamma_green = (gamma_correction.top_right.gamma_green + gamma_correction.bottom_right.gamma_green) / 2.0;
                gamma_correction.right_center.gamma_blue = (gamma_correction.top_right.gamma_blue + gamma_correction.bottom_right.gamma_blue) / 2.0;
                
                gamma_correction.bottom_center.gamma_red = (gamma_correction.bottom_right.gamma_red + gamma_correction.bottom_left.gamma_red) / 2.0;
                gamma_correction.bottom_center.gamma_green = (gamma_correction.bottom_right.gamma_green + gamma_correction.bottom_left.gamma_green) / 2.0;
                gamma_correction.bottom_center.gamma_blue = (gamma_correction.bottom_right.gamma_blue + gamma_correction.bottom_left.gamma_blue) / 2.0;
                
                gamma_correction.left_center.gamma_red = (gamma_correction.bottom_left.gamma_red + gamma_correction.top_left.gamma_red) / 2.0;
                gamma_correction.left_center.gamma_green = (gamma_correction.bottom_left.gamma_green + gamma_correction.top_left.gamma_green) / 2.0;
                gamma_correction.left_center.gamma_blue = (gamma_correction.bottom_left.gamma_blue + gamma_correction.top_left.gamma_blue) / 2.0;
            } else {
                // Legacy format - apply same gamma to all points
                double gamma_r = gc.value("gamma_red", 2.2);
                double gamma_g = gc.value("gamma_green", 2.2);
                double gamma_b = gc.value("gamma_blue", 2.2);
                
                gamma_correction.top_left.gamma_red = gamma_r;
                gamma_correction.top_left.gamma_green = gamma_g;
                gamma_correction.top_left.gamma_blue = gamma_b;
                
                gamma_correction.top_center = gamma_correction.top_left;
                gamma_correction.top_right = gamma_correction.top_left;
                gamma_correction.right_center = gamma_correction.top_left;
                gamma_correction.bottom_right = gamma_correction.top_left;
                gamma_correction.bottom_center = gamma_correction.top_left;
                gamma_correction.bottom_left = gamma_correction.top_left;
                gamma_correction.left_center = gamma_correction.top_left;
            }
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
        j["camera"]["autofocus_mode"] = camera.autofocus_mode;
        j["camera"]["lens_position"] = camera.lens_position;
        j["camera"]["awb_mode"] = camera.awb_mode;
        j["camera"]["awb_gain_red"] = camera.awb_gain_red;
        j["camera"]["awb_gain_blue"] = camera.awb_gain_blue;
        j["camera"]["awb_temperature"] = camera.awb_temperature;
        j["camera"]["analogue_gain"] = camera.analogue_gain;
        j["camera"]["digital_gain"] = camera.digital_gain;
        j["camera"]["exposure_time"] = camera.exposure_time;
        if (!camera.color_correction_matrix.empty()) {
            j["camera"]["color_correction_matrix"] = camera.color_correction_matrix;
        }
        j["camera"]["enable_scaling"] = camera.enable_scaling;
        j["camera"]["scaled_width"] = camera.scaled_width;
        j["camera"]["scaled_height"] = camera.scaled_height;
        
        j["hyperhdr"]["enabled"] = hyperhdr.enabled;
        j["hyperhdr"]["host"] = hyperhdr.host;
        j["hyperhdr"]["port"] = hyperhdr.port;
        j["hyperhdr"]["priority"] = hyperhdr.priority;
        j["hyperhdr"]["use_linear_format"] = hyperhdr.use_linear_format;
        
        j["usb"]["enabled"] = usb.enabled;
        j["usb"]["device"] = usb.device;
        j["usb"]["baudrate"] = usb.baudrate;
        
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
        j["scaling"]["offset_x"] = offset_x;
        j["scaling"]["offset_y"] = offset_y;
        j["scaling"]["flip_horizontal"] = flip_horizontal;
        j["scaling"]["flip_vertical"] = flip_vertical;
        
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
        j["color_extraction"]["method"] = color_extraction.method;
        j["color_extraction"]["horizontal_coverage_percent"] = color_extraction.horizontal_coverage_percent;
        j["color_extraction"]["vertical_coverage_percent"] = color_extraction.vertical_coverage_percent;
        j["color_extraction"]["horizontal_slices"] = color_extraction.horizontal_slices;
        j["color_extraction"]["vertical_slices"] = color_extraction.vertical_slices;
        
        j["gamma_correction"]["enabled"] = gamma_correction.enabled;
        j["gamma_correction"]["top_left"]["gamma_red"] = gamma_correction.top_left.gamma_red;
        j["gamma_correction"]["top_left"]["gamma_green"] = gamma_correction.top_left.gamma_green;
        j["gamma_correction"]["top_left"]["gamma_blue"] = gamma_correction.top_left.gamma_blue;
        j["gamma_correction"]["top_center"]["gamma_red"] = gamma_correction.top_center.gamma_red;
        j["gamma_correction"]["top_center"]["gamma_green"] = gamma_correction.top_center.gamma_green;
        j["gamma_correction"]["top_center"]["gamma_blue"] = gamma_correction.top_center.gamma_blue;
        j["gamma_correction"]["top_right"]["gamma_red"] = gamma_correction.top_right.gamma_red;
        j["gamma_correction"]["top_right"]["gamma_green"] = gamma_correction.top_right.gamma_green;
        j["gamma_correction"]["top_right"]["gamma_blue"] = gamma_correction.top_right.gamma_blue;
        j["gamma_correction"]["right_center"]["gamma_red"] = gamma_correction.right_center.gamma_red;
        j["gamma_correction"]["right_center"]["gamma_green"] = gamma_correction.right_center.gamma_green;
        j["gamma_correction"]["right_center"]["gamma_blue"] = gamma_correction.right_center.gamma_blue;
        j["gamma_correction"]["bottom_right"]["gamma_red"] = gamma_correction.bottom_right.gamma_red;
        j["gamma_correction"]["bottom_right"]["gamma_green"] = gamma_correction.bottom_right.gamma_green;
        j["gamma_correction"]["bottom_right"]["gamma_blue"] = gamma_correction.bottom_right.gamma_blue;
        j["gamma_correction"]["bottom_center"]["gamma_red"] = gamma_correction.bottom_center.gamma_red;
        j["gamma_correction"]["bottom_center"]["gamma_green"] = gamma_correction.bottom_center.gamma_green;
        j["gamma_correction"]["bottom_center"]["gamma_blue"] = gamma_correction.bottom_center.gamma_blue;
        j["gamma_correction"]["bottom_left"]["gamma_red"] = gamma_correction.bottom_left.gamma_red;
        j["gamma_correction"]["bottom_left"]["gamma_green"] = gamma_correction.bottom_left.gamma_green;
        j["gamma_correction"]["bottom_left"]["gamma_blue"] = gamma_correction.bottom_left.gamma_blue;
        j["gamma_correction"]["left_center"]["gamma_red"] = gamma_correction.left_center.gamma_red;
        j["gamma_correction"]["left_center"]["gamma_green"] = gamma_correction.left_center.gamma_green;
        j["gamma_correction"]["left_center"]["gamma_blue"] = gamma_correction.left_center.gamma_blue;
        
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
    
    if (color_extraction.method != "mean" && color_extraction.method != "dominant") {
        LOG_ERROR("Invalid color extraction method: " + color_extraction.method + " (must be 'mean' or 'dominant')");
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

