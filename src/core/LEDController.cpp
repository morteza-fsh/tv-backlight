#include "core/LEDController.h"
#include "core/ImageFrameSource.h"
#include "core/CameraFrameSource.h"
#include "utils/Logger.h"
#include "utils/PerformanceTimer.h"
#include <filesystem>
#include <thread>
#include <sstream>

namespace fs = std::filesystem;

namespace TVLED {

LEDController::LEDController(const Config& config)
    : config_(config), running_(false), initialized_(false) {
}

LEDController::~LEDController() {
    stop();
}

bool LEDController::initialize() {
    LOG_INFO("Initializing LED Controller...");
    
    if (!config_.validate()) {
        LOG_ERROR("Invalid configuration");
        return false;
    }
    
    // Create output directory
    fs::create_directories(config_.output_directory);
    
    // Setup frame source
    if (!setupFrameSource()) {
        LOG_ERROR("Failed to setup frame source");
        return false;
    }
    
    // Setup color extractor
    if (!setupColorExtractor()) {
        LOG_ERROR("Failed to setup color extractor");
        return false;
    }
    
    // Setup LED layout
    if (!setupLEDLayout()) {
        LOG_ERROR("Failed to setup LED layout");
        return false;
    }
    
    // Setup HyperHDR client (optional)
    if (config_.hyperhdr.enabled) {
        if (!setupHyperHDRClient()) {
            LOG_WARN("Failed to setup HyperHDR client, continuing without it");
        }
    }
    
    initialized_ = true;
    LOG_INFO("LED Controller initialized successfully");
    return true;
}

bool LEDController::setupFrameSource() {
    LOG_INFO("Setting up frame source...");
    
    if (config_.mode == "debug") {
        frame_source_ = std::make_unique<ImageFrameSource>(config_.input_image);
    } else if (config_.mode == "live") {
        frame_source_ = std::make_unique<CameraFrameSource>(
            config_.camera.device,
            config_.camera.width,
            config_.camera.height,
            config_.camera.fps,
            config_.camera.sensor_mode,
            config_.camera.enable_scaling,
            config_.camera.scaled_width,
            config_.camera.scaled_height
        );
    } else {
        LOG_ERROR("Unknown mode: " + config_.mode);
        return false;
    }
    
    if (!frame_source_->initialize()) {
        LOG_ERROR("Failed to initialize frame source");
        return false;
    }
    
    LOG_INFO("Frame source ready: " + frame_source_->getName());
    return true;
}

bool LEDController::setupBezierCurves() {
    LOG_INFO("Setting up Bezier curves...");
    
    // Parse all four bezier curves
    if (!top_bezier_.parse(config_.bezier.top_bezier, config_.bezier.bezier_samples)) {
        LOG_ERROR("Failed to parse top bezier curve");
        return false;
    }
    
    if (!right_bezier_.parse(config_.bezier.right_bezier, config_.bezier.bezier_samples)) {
        LOG_ERROR("Failed to parse right bezier curve");
        return false;
    }
    
    if (!bottom_bezier_.parse(config_.bezier.bottom_bezier, config_.bezier.bezier_samples)) {
        LOG_ERROR("Failed to parse bottom bezier curve");
        return false;
    }
    
    if (!left_bezier_.parse(config_.bezier.left_bezier, config_.bezier.bezier_samples)) {
        LOG_ERROR("Failed to parse left bezier curve");
        return false;
    }
    
    LOG_INFO("Bezier curves parsed successfully");
    return true;
}

bool LEDController::setupCoonsPatching(int imageWidth, int imageHeight) {
    LOG_INFO("Setting up Coons patching...");
    
    // Get bezier points
    auto top_pts = top_bezier_.getPoints();
    auto right_pts = right_bezier_.getPoints();
    auto bottom_pts_temp = bottom_bezier_.getPoints();
    auto left_pts_temp = left_bezier_.getPoints();
    
    // Reverse bottom and left to match Coons convention
    std::vector<cv::Point2f> bottom_pts(bottom_pts_temp.rbegin(), bottom_pts_temp.rend());
    std::vector<cv::Point2f> left_pts = left_pts_temp;
    
    // Find coordinate ranges for scaling
    std::vector<cv::Point2f> all_points;
    all_points.insert(all_points.end(), top_pts.begin(), top_pts.end());
    all_points.insert(all_points.end(), right_pts.begin(), right_pts.end());
    all_points.insert(all_points.end(), bottom_pts.begin(), bottom_pts.end());
    all_points.insert(all_points.end(), left_pts.begin(), left_pts.end());
    
    float svg_min_x = INFINITY, svg_max_x = -INFINITY;
    float svg_min_y = INFINITY, svg_max_y = -INFINITY;
    
    for (const auto& pt : all_points) {
        svg_min_x = std::min(svg_min_x, pt.x);
        svg_max_x = std::max(svg_max_x, pt.x);
        svg_min_y = std::min(svg_min_y, pt.y);
        svg_max_y = std::max(svg_max_y, pt.y);
    }
    
    // Scale and center
    float svg_width = svg_max_x - svg_min_x;
    float svg_height = svg_max_y - svg_min_y;
    float scale_factor = config_.scale_factor;
    
    top_bezier_.scale(scale_factor);
    right_bezier_.scale(scale_factor);
    bottom_bezier_.scale(scale_factor);
    left_bezier_.scale(scale_factor);
    
    float scaled_width = svg_width * scale_factor;
    float scaled_height = svg_height * scale_factor;
    float offset_x = std::max(0.0f, (imageWidth - scaled_width) / 2 - svg_min_x * scale_factor);
    float offset_y = std::max(0.0f, (imageHeight - scaled_height) / 2 - svg_min_y * scale_factor);
    
    top_bezier_.translate(offset_x, offset_y);
    right_bezier_.translate(offset_x, offset_y);
    bottom_bezier_.translate(offset_x, offset_y);
    left_bezier_.translate(offset_x, offset_y);
    
    // Clamp to image boundaries
    top_bezier_.clamp(0, imageWidth - 1, 0, imageHeight - 1);
    right_bezier_.clamp(0, imageWidth - 1, 0, imageHeight - 1);
    bottom_bezier_.clamp(0, imageWidth - 1, 0, imageHeight - 1);
    left_bezier_.clamp(0, imageWidth - 1, 0, imageHeight - 1);
    
    // Get final points
    top_pts = top_bezier_.getPoints();
    right_pts = right_bezier_.getPoints();
    bottom_pts_temp = bottom_bezier_.getPoints();
    left_pts_temp = left_bezier_.getPoints();
    
    // Reverse again for Coons
    bottom_pts = std::vector<cv::Point2f>(bottom_pts_temp.rbegin(), bottom_pts_temp.rend());
    left_pts = std::vector<cv::Point2f>(left_pts_temp.rbegin(), left_pts_temp.rend());
    
    // Initialize Coons patching
    coons_patching_ = std::make_unique<CoonsPatching>();
    if (!coons_patching_->initialize(top_pts, right_pts, bottom_pts, left_pts, 
                                     imageWidth, imageHeight)) {
        LOG_ERROR("Failed to initialize Coons patching");
        return false;
    }
    
    // Pre-compute all cell polygons based on mode
    cell_polygons_.clear();
    
    if (config_.color_extraction.mode == "edge_slices") {
        // Pre-compute edge slice polygons
        int h_slices = config_.color_extraction.horizontal_slices;
        int v_slices = config_.color_extraction.vertical_slices;
        int total = 2 * h_slices + 2 * v_slices;
        
        cell_polygons_.reserve(total);
        
        LOG_INFO("Pre-computing " + std::to_string(total) + " edge slice polygons...");
        PerformanceTimer timer("Edge slice polygon generation", false);
        
        float h_coverage = config_.color_extraction.horizontal_coverage_percent / 100.0f;
        float v_coverage = config_.color_extraction.vertical_coverage_percent / 100.0f;
        
        // Top edge
        for (int i = 0; i < h_slices; i++) {
            double u0 = static_cast<double>(i) / h_slices;
            double u1 = static_cast<double>(i + 1) / h_slices;
            cell_polygons_.push_back(
                coons_patching_->buildCellPolygon(u0, u1, 0.0, h_coverage, 
                                                 config_.bezier.polygon_samples)
            );
        }
        
        // Bottom edge
        for (int i = 0; i < h_slices; i++) {
            double u0 = static_cast<double>(i) / h_slices;
            double u1 = static_cast<double>(i + 1) / h_slices;
            cell_polygons_.push_back(
                coons_patching_->buildCellPolygon(u0, u1, 1.0 - h_coverage, 1.0, 
                                                 config_.bezier.polygon_samples)
            );
        }
        
        // Left edge
        for (int i = 0; i < v_slices; i++) {
            double v0 = static_cast<double>(i) / v_slices;
            double v1 = static_cast<double>(i + 1) / v_slices;
            cell_polygons_.push_back(
                coons_patching_->buildCellPolygon(0.0, v_coverage, v0, v1, 
                                                 config_.bezier.polygon_samples)
            );
        }
        
        // Right edge
        for (int i = 0; i < v_slices; i++) {
            double v0 = static_cast<double>(i) / v_slices;
            double v1 = static_cast<double>(i + 1) / v_slices;
            cell_polygons_.push_back(
                coons_patching_->buildCellPolygon(1.0 - v_coverage, 1.0, v0, v1, 
                                                 config_.bezier.polygon_samples)
            );
        }
        
        timer.stop();
        LOG_INFO("Edge slice polygon generation completed in " + 
                 std::to_string(timer.elapsedMilliseconds()) + " ms");
    } else {
        // Pre-compute grid polygons
        int rows = led_layout_->getRows();
        int cols = led_layout_->getCols();
        
        cell_polygons_.reserve(rows * cols);
        
        LOG_INFO("Pre-computing " + std::to_string(rows * cols) + " cell polygons...");
        PerformanceTimer timer("Polygon generation", false);
        
        for (int r = 0; r < rows; r++) {
            for (int c = 0; c < cols; c++) {
                double u0 = static_cast<double>(c) / cols;
                double u1 = static_cast<double>(c + 1) / cols;
                double v0 = static_cast<double>(r) / rows;
                double v1 = static_cast<double>(r + 1) / rows;
                
                cell_polygons_.push_back(
                    coons_patching_->buildCellPolygon(u0, u1, v0, v1, 
                                                     config_.bezier.polygon_samples)
                );
            }
        }
        
        timer.stop();
        LOG_INFO("Polygon generation completed in " + 
                 std::to_string(timer.elapsedMilliseconds()) + " ms");
    }
    
    return true;
}

bool LEDController::setupColorExtractor() {
    LOG_INFO("Setting up color extractor...");
    
    color_extractor_ = std::make_unique<ColorExtractor>();
    color_extractor_->setParallelProcessing(config_.performance.enable_parallel_processing);
    
    LOG_INFO("Color extractor ready");
    return true;
}

bool LEDController::setupLEDLayout() {
    LOG_INFO("Setting up LED layout...");
    
    // If using edge_slices mode, adapt LED layout accordingly
    if (config_.color_extraction.mode == "edge_slices") {
        // For edge slices: top + bottom + left + right
        int total_leds = 2 * config_.color_extraction.horizontal_slices + 
                        2 * config_.color_extraction.vertical_slices;
        
        // Create a linear layout for edge slices
        // Top: horizontal_slices, Bottom: horizontal_slices, Left: vertical_slices, Right: vertical_slices
        led_layout_ = std::make_unique<LEDLayout>(
            LEDLayout::fromHyperHDR(
                config_.color_extraction.horizontal_slices,
                config_.color_extraction.horizontal_slices,
                config_.color_extraction.vertical_slices,
                config_.color_extraction.vertical_slices
            )
        );
        
        LOG_INFO("LED layout configured for edge_slices mode: " + 
                 std::to_string(total_leds) + " LEDs");
    } else if (config_.led_layout.format == "grid") {
        led_layout_ = std::make_unique<LEDLayout>(
            LEDLayout::fromGrid(config_.led_layout.grid_rows, config_.led_layout.grid_cols)
        );
    } else if (config_.led_layout.format == "hyperhdr") {
        led_layout_ = std::make_unique<LEDLayout>(
            LEDLayout::fromHyperHDR(
                config_.led_layout.hyperhdr_top,
                config_.led_layout.hyperhdr_bottom,
                config_.led_layout.hyperhdr_left,
                config_.led_layout.hyperhdr_right
            )
        );
    } else {
        LOG_ERROR("Unknown LED layout format: " + config_.led_layout.format);
        return false;
    }
    
    return true;
}

bool LEDController::setupHyperHDRClient() {
    LOG_INFO("Setting up HyperHDR client...");
    
    hyperhdr_client_ = std::make_unique<HyperHDRClient>(
        config_.hyperhdr.host,
        config_.hyperhdr.port,
        config_.hyperhdr.priority
    );
    
    if (!hyperhdr_client_->connect()) {
        LOG_ERROR("Failed to connect to HyperHDR");
        return false;
    }
    
    LOG_INFO("HyperHDR client ready");
    return true;
}

bool LEDController::processFrame(const cv::Mat& frame, std::vector<cv::Vec3b>& colors) {
    // Setup Coons patching if not already done (needs frame dimensions)
    if (!coons_patching_) {
        if (!setupBezierCurves()) {
            return false;
        }
        if (!setupCoonsPatching(frame.cols, frame.rows)) {
            return false;
        }
    }
    
    // Extract colors using pre-computed polygons (works for both modes)
    colors = color_extractor_->extractColors(frame, cell_polygons_);
    
    return !colors.empty();
}

bool LEDController::processSingleFrame(bool saveDebugImages) {
    if (!initialized_) {
        LOG_ERROR("LED Controller not initialized");
        return false;
    }
    
    PerformanceTimer total_timer("Total frame processing", false);
    
    // Get frame
    cv::Mat frame;
    if (!frame_source_->getFrame(frame)) {
        LOG_ERROR("Failed to get frame");
        return false;
    }
    
    LOG_INFO("Processing frame: " + std::to_string(frame.cols) + "x" + 
             std::to_string(frame.rows));
    
    // Process frame
    std::vector<cv::Vec3b> colors;
    if (!processFrame(frame, colors)) {
        LOG_ERROR("Failed to process frame");
        return false;
    }
    
    // Log colors
    std::stringstream ss;
    ss << "RGB colors per LED: ";
    for (size_t i = 0; i < std::min(colors.size(), size_t(10)); i++) {
        const auto& c = colors[i];
        ss << "(" << static_cast<int>(c[0]) << "," 
           << static_cast<int>(c[1]) << "," 
           << static_cast<int>(c[2]) << ") ";
    }
    if (colors.size() > 10) {
        ss << "... (total: " << colors.size() << ")";
    }
    LOG_INFO(ss.str());
    
    // Send to HyperHDR
    if (hyperhdr_client_ && hyperhdr_client_->isConnected()) {
        if (hyperhdr_client_->sendColors(colors)) {
            LOG_INFO("Sent " + std::to_string(colors.size()) + " colors to HyperHDR");
        } else {
            LOG_WARN("Failed to send colors to HyperHDR");
        }
    }
    
    // Save debug images
    if (saveDebugImages) {
        saveDebugBoundaries(frame);
        saveColorGrid(colors);
    }
    
    total_timer.stop();
    LOG_INFO("Frame processed in " + std::to_string(total_timer.elapsedMilliseconds()) + " ms");
    
    return true;
}

int LEDController::run() {
    if (!initialized_) {
        LOG_ERROR("LED Controller not initialized");
        return -1;
    }
    
    running_ = true;
    int frame_count = 0;
    
    LOG_INFO("Starting main processing loop...");
    LOG_INFO("Press Ctrl+C to stop");
    
    auto loop_start = std::chrono::high_resolution_clock::now();
    
    while (running_) {
        if (!processSingleFrame(false)) {
            LOG_ERROR("Frame processing failed");
            break;
        }
        
        frame_count++;
        
        // FPS throttling
        if (config_.performance.target_fps > 0) {
            auto frame_duration = std::chrono::milliseconds(1000 / config_.performance.target_fps);
            std::this_thread::sleep_for(frame_duration);
        }
        
        // Log FPS every 100 frames
        if (frame_count % 100 == 0) {
            auto now = std::chrono::high_resolution_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - loop_start);
            double fps = frame_count * 1000.0 / elapsed.count();
            LOG_INFO("Processed " + std::to_string(frame_count) + " frames, " +
                    std::to_string(fps) + " FPS");
        }
    }
    
    auto loop_end = std::chrono::high_resolution_clock::now();
    auto total_elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(loop_end - loop_start);
    double avg_fps = frame_count * 1000.0 / total_elapsed.count();
    
    LOG_INFO("Processing complete: " + std::to_string(frame_count) + 
             " frames in " + std::to_string(total_elapsed.count()) + 
             " ms (avg " + std::to_string(avg_fps) + " FPS)");
    
    return frame_count;
}

void LEDController::stop() {
    running_ = false;
}

void LEDController::saveDebugBoundaries(const cv::Mat& frame) {
    cv::Mat debug_img = frame.clone();
    
    // Draw boundary curves
    std::vector<cv::Point> top_int, right_int, bottom_int, left_int;
    
    for (const auto& pt : top_bezier_.getPoints()) {
        top_int.push_back(cv::Point(static_cast<int>(pt.x), static_cast<int>(pt.y)));
    }
    for (const auto& pt : right_bezier_.getPoints()) {
        right_int.push_back(cv::Point(static_cast<int>(pt.x), static_cast<int>(pt.y)));
    }
    for (const auto& pt : bottom_bezier_.getPoints()) {
        bottom_int.push_back(cv::Point(static_cast<int>(pt.x), static_cast<int>(pt.y)));
    }
    for (const auto& pt : left_bezier_.getPoints()) {
        left_int.push_back(cv::Point(static_cast<int>(pt.x), static_cast<int>(pt.y)));
    }
    
    cv::polylines(debug_img, top_int, false, cv::Scalar(255, 0, 0), 
                 config_.visualization.debug_boundary_thickness);
    cv::polylines(debug_img, right_int, false, cv::Scalar(0, 255, 0), 
                 config_.visualization.debug_boundary_thickness);
    cv::polylines(debug_img, bottom_int, false, cv::Scalar(0, 0, 255), 
                 config_.visualization.debug_boundary_thickness);
    cv::polylines(debug_img, left_int, false, cv::Scalar(255, 255, 0), 
                 config_.visualization.debug_boundary_thickness);
    
    // Mark corners
    int radius = config_.visualization.debug_corner_radius;
    cv::circle(debug_img, top_int.front(), radius, cv::Scalar(255, 255, 255), -1);
    cv::circle(debug_img, top_int.back(), radius, cv::Scalar(255, 255, 255), -1);
    cv::circle(debug_img, bottom_int.front(), radius, cv::Scalar(0, 0, 0), -1);
    cv::circle(debug_img, bottom_int.back(), radius, cv::Scalar(0, 0, 0), -1);
    
    // If edge_slices mode, visualize the edge regions
    if (config_.color_extraction.mode == "edge_slices" && coons_patching_) {
        float h_cov = config_.color_extraction.horizontal_coverage_percent / 100.0f;
        float v_cov = config_.color_extraction.vertical_coverage_percent / 100.0f;
        
        // Draw semi-transparent overlay for edge regions
        cv::Mat overlay = debug_img.clone();
        
        // Generate and draw all edge slice polygons
        int h_slices = config_.color_extraction.horizontal_slices;
        int v_slices = config_.color_extraction.vertical_slices;
        
        // Top edge
        for (int i = 0; i < h_slices; i++) {
            double u0 = static_cast<double>(i) / h_slices;
            double u1 = static_cast<double>(i + 1) / h_slices;
            auto poly = coons_patching_->buildCellPolygon(u0, u1, 0.0, h_cov, 
                                                          config_.bezier.polygon_samples);
            cv::polylines(overlay, poly, true, cv::Scalar(255, 100, 100), 2);
        }
        
        // Bottom edge
        for (int i = 0; i < h_slices; i++) {
            double u0 = static_cast<double>(i) / h_slices;
            double u1 = static_cast<double>(i + 1) / h_slices;
            auto poly = coons_patching_->buildCellPolygon(u0, u1, 1.0 - h_cov, 1.0, 
                                                          config_.bezier.polygon_samples);
            cv::polylines(overlay, poly, true, cv::Scalar(100, 100, 255), 2);
        }
        
        // Left edge
        for (int i = 0; i < v_slices; i++) {
            double v0 = static_cast<double>(i) / v_slices;
            double v1 = static_cast<double>(i + 1) / v_slices;
            auto poly = coons_patching_->buildCellPolygon(0.0, v_cov, v0, v1, 
                                                          config_.bezier.polygon_samples);
            cv::polylines(overlay, poly, true, cv::Scalar(100, 255, 100), 2);
        }
        
        // Right edge
        for (int i = 0; i < v_slices; i++) {
            double v0 = static_cast<double>(i) / v_slices;
            double v1 = static_cast<double>(i + 1) / v_slices;
            auto poly = coons_patching_->buildCellPolygon(1.0 - v_cov, 1.0, v0, v1, 
                                                          config_.bezier.polygon_samples);
            cv::polylines(overlay, poly, true, cv::Scalar(255, 255, 100), 2);
        }
        
        cv::addWeighted(debug_img, 0.7, overlay, 0.3, 0, debug_img);
    }
    
    std::string path = config_.output_directory + "/debug_boundaries.png";
    cv::imwrite(path, debug_img);
    LOG_INFO("Saved debug boundaries to " + path);
}

void LEDController::saveColorGrid(const std::vector<cv::Vec3b>& colors) {
    int cell_w = config_.visualization.grid_cell_width;
    int cell_h = config_.visualization.grid_cell_height;
    
    cv::Mat color_grid;
    
    if (config_.color_extraction.mode == "edge_slices") {
        // For edge slices, create a linear strip visualization
        int total_leds = static_cast<int>(colors.size());
        int cols_per_row = 20;  // Display 20 LEDs per row for readability
        int rows = (total_leds + cols_per_row - 1) / cols_per_row;
        
        color_grid = cv::Mat::zeros(rows * cell_h, cols_per_row * cell_w, CV_8UC3);
        
        for (int i = 0; i < total_leds; i++) {
            const auto& color = colors[i];
            
            int row = i / cols_per_row;
            int col = i % cols_per_row;
            
            int x_start = col * cell_w;
            int y_start = row * cell_h;
            int x_end = x_start + cell_w;
            int y_end = y_start + cell_h;
            
            // Convert RGB to BGR for OpenCV
            cv::rectangle(color_grid, cv::Point(x_start, y_start), cv::Point(x_end, y_end),
                         cv::Scalar(color[2], color[1], color[0]), -1);
            
            // Border
            cv::rectangle(color_grid, cv::Point(x_start, y_start), cv::Point(x_end, y_end),
                         cv::Scalar(255, 255, 255), config_.color_settings.border_thickness);
            
            // Index label
            if (config_.color_settings.show_coordinates) {
                std::string text = std::to_string(i);
                cv::putText(color_grid, text, cv::Point(x_start + 2, y_start + 15),
                           cv::FONT_HERSHEY_SIMPLEX, config_.color_settings.coordinate_font_scale,
                           cv::Scalar(255, 255, 255), 1);
            }
        }
    } else {
        // Grid mode visualization
        int rows = led_layout_->getRows();
        int cols = led_layout_->getCols();
        
        color_grid = cv::Mat::zeros(rows * cell_h, cols * cell_w, CV_8UC3);
        
        for (int r = 0; r < rows; r++) {
            for (int c = 0; c < cols; c++) {
                int idx = r * cols + c;
                if (idx >= static_cast<int>(colors.size())) break;
                
                const auto& color = colors[idx];
                
                int x_start = c * cell_w;
                int y_start = r * cell_h;
                int x_end = x_start + cell_w;
                int y_end = y_start + cell_h;
                
                // Convert RGB to BGR for OpenCV
                cv::rectangle(color_grid, cv::Point(x_start, y_start), cv::Point(x_end, y_end),
                             cv::Scalar(color[2], color[1], color[0]), -1);
                
                // Border
                cv::rectangle(color_grid, cv::Point(x_start, y_start), cv::Point(x_end, y_end),
                             cv::Scalar(255, 255, 255), config_.color_settings.border_thickness);
                
                // Coordinates
                if (config_.color_settings.show_coordinates) {
                    std::string text = std::to_string(r) + "," + std::to_string(c);
                    cv::putText(color_grid, text, cv::Point(x_start + 2, y_start + 15),
                               cv::FONT_HERSHEY_SIMPLEX, config_.color_settings.coordinate_font_scale,
                               cv::Scalar(255, 255, 255), 1);
                }
            }
        }
    }
    
    std::string path = config_.output_directory + "/dominant_color_grid.png";
    cv::imwrite(path, color_grid);
    LOG_INFO("Saved color grid to " + path);
}

} // namespace TVLED

