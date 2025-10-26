#include <opencv2/opencv.hpp>
#include <iostream>
#include <vector>
#include <cmath>
#include <regex>
#include <string>
#include <fstream>
#include <algorithm>
#include <filesystem>
#include <chrono>
#include <memory>
#include <map>
#include <nlohmann/json.hpp>

using namespace cv;
using namespace std;
using namespace std::chrono;
using json = nlohmann::json;

namespace fs = std::filesystem;

// Structure to cache cumulative arc lengths for fast interpolation
struct PolylineCache {
    vector<double> cumulative_lengths;
    double total_length;
    const vector<Point2f>* poly_ptr;
    
    PolylineCache(const vector<Point2f>& poly) : poly_ptr(&poly) {
        cumulative_lengths.reserve(poly.size());
        cumulative_lengths.push_back(0.0);
        total_length = 0.0;
        
        for (size_t i = 1; i < poly.size(); i++) {
            double dist = norm(poly[i] - poly[i-1]);
            total_length += dist;
            cumulative_lengths.push_back(total_length);
        }
    }
    
    // Fast interpolation using cached lengths
    Point2f interp(double t) const {
        double d = max(0.0, min(1.0, t)) * total_length;
        
        // Binary search for the segment (faster than linear)
        size_t i = 0;
        if (cumulative_lengths.size() > 2) {
            // Binary search
            size_t left = 0, right = cumulative_lengths.size() - 1;
            while (left < right - 1) {
                size_t mid = (left + right) / 2;
                if (d <= cumulative_lengths[mid]) {
                    right = mid;
                } else {
                    left = mid;
                }
            }
            i = left;
        }
        i = min(i, poly_ptr->size() - 2);
        
        double span = cumulative_lengths[i + 1] - cumulative_lengths[i];
        double w = (span == 0) ? 0 : (d - cumulative_lengths[i]) / span;
        
        const auto& poly = *poly_ptr;
        return (1 - w) * poly[i] + w * poly[i + 1];
    }
};

// Helper function to calculate cumulative arc length (kept for compatibility)
pair<vector<double>, double> cumlen(const vector<Point2f>& poly) {
    vector<double> s;
    s.push_back(0.0);
    double total = 0.0;
    
    for (size_t i = 1; i < poly.size(); i++) {
        double dist = norm(poly[i] - poly[i-1]);
        total += dist;
        s.push_back(total);
    }
    
    return {s, total};
}

// Interpolate along a polyline at parameter t (0 to 1)
Point2f interp(const vector<Point2f>& poly, double t) {
    auto [s, L] = cumlen(poly);
    double d = max(0.0, min(1.0, t)) * L;
    
    // Find the segment
    int i = 0;
    for (size_t j = 0; j < s.size() - 1; j++) {
        if (d <= s[j + 1]) {
            i = j;
            break;
        }
    }
    i = max(0, min(i, (int)poly.size() - 2));
    
    double span = s[i + 1] - s[i];
    double w = (span == 0) ? 0 : (d - s[i]) / span;
    
    return (1 - w) * poly[i] + w * poly[i + 1];
}

// Configuration structure
struct Config {
    // Input/Output
    string input_image = "img2.png";
    string output_directory = "output";
    
    // Bezier curves
    string left_bezier;
    string bottom_bezier;
    string right_bezier;
    string top_bezier;
    
    // Bezier settings
    bool use_direct_bezier_curves = true;
    int bezier_samples = 50;
    int polygon_samples = 15;
    
    // Scaling
    float scale_factor = 2.0f;
    
    // Grid settings
    int grid_rows = 5;
    int grid_cols = 8;
    
    // Visualization
    int grid_cell_width = 60;
    int grid_cell_height = 40;
    int debug_boundary_thickness = 3;
    int debug_corner_radius = 10;
    
    // Color settings
    bool show_coordinates = true;
    float coordinate_font_scale = 0.4f;
    int border_thickness = 1;
    
    // Performance
    bool enable_parallel_processing = true;
    int parallel_chunk_size = 4;
};

// Function to parse JSON config file
Config parseConfigFile(const string& filename) {
    Config config;
    
    try {
        ifstream file(filename);
        if (!file.is_open()) {
            cerr << "Error: Could not open config file " << filename << endl;
            return config;
        }
        
        cout << "Loading configuration from " << filename << "..." << endl;
        
        json j;
        file >> j;
        
        // Parse basic settings
        config.input_image = j.value("input_image", "img2.png");
        config.output_directory = j.value("output_directory", "output");
        
        // Parse bezier curves
        if (j.contains("bezier_curves")) {
            auto bezier_curves = j["bezier_curves"];
            config.left_bezier = bezier_curves.value("left_bezier", "");
            config.bottom_bezier = bezier_curves.value("bottom_bezier", "");
            config.right_bezier = bezier_curves.value("right_bezier", "");
            config.top_bezier = bezier_curves.value("top_bezier", "");
        }
        
        // Parse bezier settings
        if (j.contains("bezier_settings")) {
            auto bezier_settings = j["bezier_settings"];
            config.use_direct_bezier_curves = bezier_settings.value("use_direct_bezier_curves", true);
            config.bezier_samples = bezier_settings.value("bezier_samples", 50);
            config.polygon_samples = bezier_settings.value("polygon_samples", 15);
        }
        
        // Parse scaling
        if (j.contains("scaling")) {
            auto scaling = j["scaling"];
            config.scale_factor = scaling.value("scale_factor", 2.0f);
        }
        
        // Parse grid settings
        if (j.contains("grid")) {
            auto grid = j["grid"];
            config.grid_rows = grid.value("rows", 5);
            config.grid_cols = grid.value("cols", 8);
        }
        
        // Parse visualization settings
        if (j.contains("visualization")) {
            auto viz = j["visualization"];
            config.grid_cell_width = viz.value("grid_cell_width", 60);
            config.grid_cell_height = viz.value("grid_cell_height", 40);
            config.debug_boundary_thickness = viz.value("debug_boundary_thickness", 3);
            config.debug_corner_radius = viz.value("debug_corner_radius", 10);
        }
        
        // Parse color settings
        if (j.contains("color_settings")) {
            auto color = j["color_settings"];
            config.show_coordinates = color.value("show_coordinates", true);
            config.coordinate_font_scale = color.value("coordinate_font_scale", 0.4f);
            config.border_thickness = color.value("border_thickness", 1);
        }
        
        // Parse performance settings
        if (j.contains("performance")) {
            auto perf = j["performance"];
            config.enable_parallel_processing = perf.value("enable_parallel_processing", true);
            config.parallel_chunk_size = perf.value("parallel_chunk_size", 4);
        }
        
        file.close();
        cout << "Configuration loaded successfully from JSON" << endl;
        
    } catch (const json::exception& e) {
        cerr << "Error parsing JSON config: " << e.what() << endl;
        cerr << "Using default configuration values" << endl;
    }
    
    return config;
}

// Parse a single cubic Bézier curve from SVG path format
vector<Point2f> parseSingleBezierCurve(const string& bezierPath, int numSamples = 50) {
    vector<Point2f> points;
    
    // Extract start point (M command)
    regex move_regex(R"(M\s*([\d.-]+)\s+([\d.-]+))");
    smatch move_match;
    if (!regex_search(bezierPath, move_match, move_regex)) {
        throw runtime_error("Invalid Bézier curve format");
    }
    
    float start_x = stof(move_match[1].str());
    float start_y = stof(move_match[2].str());
    
    // Extract control points and end point (C command)
    regex curve_regex(R"(C\s*([\d.-]+)\s+([\d.-]+)\s+([\d.-]+)\s+([\d.-]+)\s+([\d.-]+)\s+([\d.-]+))");
    smatch curve_match;
    if (!regex_search(bezierPath, curve_match, curve_regex)) {
        throw runtime_error("Invalid Bézier curve format");
    }
    
    float x1 = stof(curve_match[1].str());
    float y1 = stof(curve_match[2].str());
    float x2 = stof(curve_match[3].str());
    float y2 = stof(curve_match[4].str());
    float x3 = stof(curve_match[5].str());
    float y3 = stof(curve_match[6].str());
    
    // Sample points along the cubic bezier curve
    for (int i = 0; i < numSamples; i++) {
        float t = (float)i / (numSamples - 1);
        // Cubic bezier formula: B(t) = (1-t)³P0 + 3(1-t)²tP1 + 3(1-t)t²P2 + t³P3
        float x = pow(1-t, 3) * start_x + 3*pow(1-t, 2)*t * x1 + 3*(1-t)*pow(t, 2) * x2 + pow(t, 3) * x3;
        float y = pow(1-t, 3) * start_y + 3*pow(1-t, 2)*t * y1 + 3*(1-t)*pow(t, 2) * y2 + pow(t, 3) * y3;
        points.push_back(Point2f(x, y));
    }
    
    return points;
}

// Global boundary curves
vector<Point2f> top_pts, right_pts, bottom_pts, left_pts;
vector<Point2f> bottom_pts_corrected, left_pts_corrected;
Point2f P00, P10, P11, P01;
int W, H;

// Cached polylines for fast interpolation
unique_ptr<PolylineCache> top_cache, bottom_cache, left_cache, right_cache;

// Coons patch boundary functions - using cached interpolation
Point2f C_top(double u) { return top_cache ? top_cache->interp(u) : interp(top_pts, u); }
Point2f C_bottom(double u) { return bottom_cache ? bottom_cache->interp(u) : interp(bottom_pts_corrected, u); }
Point2f D_left(double v) { return left_cache ? left_cache->interp(v) : interp(left_pts_corrected, v); }
Point2f D_right(double v) { return right_cache ? right_cache->interp(v) : interp(right_pts, v); }

// Coons patch interpolation
Point2f coons(double u, double v) {
    Point2f c0 = C_top(u);
    Point2f c1 = C_bottom(u);
    Point2f d0 = D_left(v);
    Point2f d1 = D_right(v);
    Point2f B = (1-u)*(1-v)*P00 + u*(1-v)*P10 + u*v*P11 + (1-u)*v*P01;
    return (1-v)*c0 + v*c1 + (1-u)*d0 + u*d1 - B;
}

// Build curved cell polygon using Coons patch - OPTIMIZED
vector<Point> buildCurvedCellPolygon(double u0, double u1, double v0, double v1, int samples = 40) {
    vector<Point> poly;
    poly.reserve(samples * 4);
    
    double du = (u1 - u0) / (samples - 1);
    double dv = (v1 - v0) / (samples - 1);
    
    // Top edge: u from u0->u1 at v=v0
    for (int i = 0; i < samples; i++) {
        double u = u0 + du * i;
        Point2f pt = coons(u, v0);
        poly.push_back(Point((int)clamp(pt.x, 0.0f, (float)(W-1)), 
                            (int)clamp(pt.y, 0.0f, (float)(H-1))));
    }
    
    // Right edge: v from v0->v1 at u=u1
    for (int i = 1; i < samples; i++) {
        double v = v0 + dv * i;
        Point2f pt = coons(u1, v);
        poly.push_back(Point((int)clamp(pt.x, 0.0f, (float)(W-1)), 
                            (int)clamp(pt.y, 0.0f, (float)(H-1))));
    }
    
    // Bottom edge: u from u1->u0 at v=v1 (reverse)
    for (int i = 1; i < samples; i++) {
        double u = u1 - du * i;
        Point2f pt = coons(u, v1);
        poly.push_back(Point((int)clamp(pt.x, 0.0f, (float)(W-1)), 
                            (int)clamp(pt.y, 0.0f, (float)(H-1))));
    }
    
    // Left edge: v from v1->v0 at u=u0 (reverse)
    for (int i = 1; i < samples; i++) {
        double v = v1 - dv * i;
        Point2f pt = coons(u0, v);
        poly.push_back(Point((int)clamp(pt.x, 0.0f, (float)(W-1)), 
                            (int)clamp(pt.y, 0.0f, (float)(H-1))));
    }
    
    return poly;
}

int main() {
    // Load configuration
    Config config = parseConfigFile("config.json");
    
    // Read image
    Mat img = imread(config.input_image);
    if (img.empty()) {
        cerr << "Error: Could not read image " << config.input_image << endl;
        return -1;
    }
    
    H = img.rows;
    W = img.cols;
    cout << "Image dimensions: " << W << " x " << H << endl;
    
    // Validate bezier curves are loaded
    if (config.left_bezier.empty() || config.bottom_bezier.empty() || 
        config.right_bezier.empty() || config.top_bezier.empty()) {
        cerr << "Error: Bezier curves not properly loaded from config file" << endl;
        return -1;
    }
    
    cout << "Loaded bezier curves from config:" << endl;
    cout << "  Left: " << config.left_bezier << endl;
    cout << "  Bottom: " << config.bottom_bezier << endl;
    cout << "  Right: " << config.right_bezier << endl;
    cout << "  Top: " << config.top_bezier << endl;
    
    if (config.use_direct_bezier_curves) {
        cout << "Using direct Bézier curves for boundaries" << endl;
        
        // Parse the 4 Bézier curves directly
        top_pts = parseSingleBezierCurve(config.top_bezier, config.bezier_samples);
        right_pts = parseSingleBezierCurve(config.right_bezier, config.bezier_samples);
        vector<Point2f> bottom_pts_temp = parseSingleBezierCurve(config.bottom_bezier, config.bezier_samples);
        vector<Point2f> left_pts_temp = parseSingleBezierCurve(config.left_bezier, config.bezier_samples);
        
        // Reverse curves to match Coons patch convention
        bottom_pts = vector<Point2f>(bottom_pts_temp.rbegin(), bottom_pts_temp.rend());
        left_pts = left_pts_temp;
    }
    
    // Combine all points for scaling calculation
    vector<Point2f> all_points;
    all_points.insert(all_points.end(), top_pts.begin(), top_pts.end());
    all_points.insert(all_points.end(), right_pts.begin(), right_pts.end());
    all_points.insert(all_points.end(), bottom_pts.begin(), bottom_pts.end());
    all_points.insert(all_points.end(), left_pts.begin(), left_pts.end());
    
    // Check coordinate ranges
    float svg_min_x = INFINITY, svg_max_x = -INFINITY;
    float svg_min_y = INFINITY, svg_max_y = -INFINITY;
    
    for (const auto& pt : all_points) {
        svg_min_x = min(svg_min_x, pt.x);
        svg_max_x = max(svg_max_x, pt.x);
        svg_min_y = min(svg_min_y, pt.y);
        svg_max_y = max(svg_max_y, pt.y);
    }
    
    cout << "Coordinate ranges: X(" << svg_min_x << ", " << svg_max_x << "), Y(" 
         << svg_min_y << ", " << svg_max_y << ")" << endl;
    
    // Scale to fit the image better
    float svg_width = svg_max_x - svg_min_x;
    float svg_height = svg_max_y - svg_min_y;
    float scale_factor = config.scale_factor;
    cout << "Scaling by factor: " << scale_factor << endl;
    
    // Scale all boundary curves
    for (auto& pt : top_pts) pt *= scale_factor;
    for (auto& pt : right_pts) pt *= scale_factor;
    for (auto& pt : bottom_pts) pt *= scale_factor;
    for (auto& pt : left_pts) pt *= scale_factor;
    
    // Center the scaled curves in the image
    float scaled_width = svg_width * scale_factor;
    float scaled_height = svg_height * scale_factor;
    float offset_x = max(0.0f, (W - scaled_width) / 2 - svg_min_x * scale_factor);
    float offset_y = max(0.0f, (H - scaled_height) / 2 - svg_min_y * scale_factor);
    
    for (auto& pt : top_pts) { pt.x += offset_x; pt.y += offset_y; }
    for (auto& pt : right_pts) { pt.x += offset_x; pt.y += offset_y; }
    for (auto& pt : bottom_pts) { pt.x += offset_x; pt.y += offset_y; }
    for (auto& pt : left_pts) { pt.x += offset_x; pt.y += offset_y; }
    
    // Clamp coordinates to image boundaries
    for (auto& pt : top_pts) { pt.x = clamp(pt.x, 0.0f, (float)(W-1)); pt.y = clamp(pt.y, 0.0f, (float)(H-1)); }
    for (auto& pt : right_pts) { pt.x = clamp(pt.x, 0.0f, (float)(W-1)); pt.y = clamp(pt.y, 0.0f, (float)(H-1)); }
    for (auto& pt : bottom_pts) { pt.x = clamp(pt.x, 0.0f, (float)(W-1)); pt.y = clamp(pt.y, 0.0f, (float)(H-1)); }
    for (auto& pt : left_pts) { pt.x = clamp(pt.x, 0.0f, (float)(W-1)); pt.y = clamp(pt.y, 0.0f, (float)(H-1)); }
    
    // Create combined path for visualization
    vector<Point2f> path_points;
    path_points.insert(path_points.end(), top_pts.begin(), top_pts.end());
    path_points.insert(path_points.end(), right_pts.begin(), right_pts.end());
    path_points.insert(path_points.end(), bottom_pts.begin(), bottom_pts.end());
    path_points.insert(path_points.end(), left_pts.begin(), left_pts.end());
    
    // Debug: visualize the boundary curves
    cout << "Top boundary points: " << top_pts.size() << ", from " << top_pts[0] << " to " << top_pts.back() << endl;
    cout << "Right boundary points: " << right_pts.size() << ", from " << right_pts[0] << " to " << right_pts.back() << endl;
    cout << "Bottom boundary points: " << bottom_pts.size() << ", from " << bottom_pts[0] << " to " << bottom_pts.back() << endl;
    cout << "Left boundary points: " << left_pts.size() << ", from " << left_pts[0] << " to " << left_pts.back() << endl;
    
    // Create output directory
    string output_dir = config.output_directory;
    fs::create_directories(output_dir);
    
    // Create debug visualization of the four boundaries
    Mat debug_img = img.clone();
    vector<Point> top_int, right_int, bottom_int, left_int;
    for (const auto& pt : top_pts) top_int.push_back(Point((int)pt.x, (int)pt.y));
    for (const auto& pt : right_pts) right_int.push_back(Point((int)pt.x, (int)pt.y));
    for (const auto& pt : bottom_pts) bottom_int.push_back(Point((int)pt.x, (int)pt.y));
    for (const auto& pt : left_pts) left_int.push_back(Point((int)pt.x, (int)pt.y));
    
    polylines(debug_img, top_int, false, Scalar(255, 0, 0), config.debug_boundary_thickness);    // blue
    polylines(debug_img, right_int, false, Scalar(0, 255, 0), config.debug_boundary_thickness);  // green
    polylines(debug_img, bottom_int, false, Scalar(0, 0, 255), config.debug_boundary_thickness); // red
    polylines(debug_img, left_int, false, Scalar(255, 255, 0), config.debug_boundary_thickness); // cyan
    
    // Mark corners
    circle(debug_img, Point((int)top_pts[0].x, (int)top_pts[0].y), config.debug_corner_radius, Scalar(255, 255, 255), -1);
    circle(debug_img, Point((int)top_pts.back().x, (int)top_pts.back().y), config.debug_corner_radius, Scalar(255, 255, 255), -1);
    circle(debug_img, Point((int)bottom_pts[0].x, (int)bottom_pts[0].y), config.debug_corner_radius, Scalar(0, 0, 0), -1);
    circle(debug_img, Point((int)bottom_pts.back().x, (int)bottom_pts.back().y), config.debug_corner_radius, Scalar(0, 0, 0), -1);
    
    string debug_path = output_dir + "/debug_boundaries.png";
    imwrite(debug_path, debug_img);
    cout << "Saved " << debug_path << endl;
    
    // Reverse bottom_pts to go L->R and left_pts to go T->B
    bottom_pts_corrected = vector<Point2f>(bottom_pts.rbegin(), bottom_pts.rend());
    left_pts_corrected = vector<Point2f>(left_pts.rbegin(), left_pts.rend());
    
    P00 = top_pts[0];                      // TL
    P10 = top_pts.back();                  // TR
    P11 = bottom_pts_corrected.back();     // BR
    P01 = bottom_pts_corrected[0];         // BL
    
    // Verify corners match
    cout << "Coons patch corners:" << endl;
    cout << "  P00 (TL): " << P00 << " = top[0]" << endl;
    cout << "  P10 (TR): " << P10 << " = top[-1]" << endl;
    cout << "  P11 (BR): " << P11 << " = bottom[0]" << endl;
    cout << "  P01 (BL): " << P01 << " = bottom[-1]" << endl;
    
    // Initialize cached polylines for ultra-fast Coons patch interpolation
    cout << "Initializing cached polylines..." << endl;
    top_cache = make_unique<PolylineCache>(top_pts);
    bottom_cache = make_unique<PolylineCache>(bottom_pts_corrected);
    left_cache = make_unique<PolylineCache>(left_pts_corrected);
    right_cache = make_unique<PolylineCache>(right_pts);
    cout << "Caches initialized" << endl;
    
    // Grid + crop + dominant color - ULTRA OPTIMIZED VERSION
    int rows = config.grid_rows, cols = config.grid_cols;
    
    vector<Vec3b> dominant_colors;
    dominant_colors.resize(rows * cols);
    
    cout << "Calculating dominant colors for " << rows << "x" << cols << " grid..." << endl;
    auto total_start = high_resolution_clock::now();
    
    // OPTIMIZATION 1: Pre-compute all cell polygons (single-threaded, but fast)
    auto poly_start = high_resolution_clock::now();
    vector<vector<Point>> all_polygons(rows * cols);
    vector<Rect> all_bboxes(rows * cols);
    
    for (int r = 0; r < rows; r++) {
        for (int c = 0; c < cols; c++) {
            int idx = r * cols + c;
            double u0 = (double)c / cols;
            double u1 = (double)(c + 1) / cols;
            double v0 = (double)r / rows;
            double v1 = (double)(r + 1) / rows;
            
            // Build curved polygon (reduced samples)
            all_polygons[idx] = buildCurvedCellPolygon(u0, u1, v0, v1, config.polygon_samples);
            all_bboxes[idx] = boundingRect(all_polygons[idx]);
            all_bboxes[idx] &= Rect(0, 0, W, H);
        }
    }
    auto poly_end = high_resolution_clock::now();
    cout << "  Polygon generation: " << duration_cast<milliseconds>(poly_end - poly_start).count() << " ms" << endl;
    
    // OPTIMIZATION 2: Use integral images for ultra-fast mean calculation
    auto integral_start = high_resolution_clock::now();
    Mat integral_img;
    Mat integral_count;
    vector<Mat> channels(3);
    split(img, channels);
    
    vector<Mat> integral_channels(3);
    for (int i = 0; i < 3; i++) {
        integral(channels[i], integral_channels[i], CV_64F);
    }
    auto integral_end = high_resolution_clock::now();
    cout << "  Integral image creation: " << duration_cast<milliseconds>(integral_end - integral_start).count() << " ms" << endl;
    
    // OPTIMIZATION 3: Parallel color calculation with optimized sampling
    auto calc_start = high_resolution_clock::now();
    
    #pragma omp parallel for schedule(dynamic, 4)
    for (int idx = 0; idx < rows * cols; idx++) {
        const vector<Point>& poly = all_polygons[idx];
        const Rect& bbox = all_bboxes[idx];
        
        if (bbox.width <= 0 || bbox.height <= 0) {
            dominant_colors[idx] = Vec3b(0, 0, 0);
            continue;
        }
        
        // OPTIMIZATION: Use scanline-based approach instead of full mask
        // This is much faster for computing means
        long long sum_b = 0, sum_g = 0, sum_r = 0;
        int pixel_count = 0;
        
        // Create minimal mask for scanline processing
        Mat mask = Mat::zeros(bbox.height, bbox.width, CV_8UC1);
        vector<Point> poly_relative;
        poly_relative.reserve(poly.size());
        for (const auto& pt : poly) {
            poly_relative.push_back(Point(pt.x - bbox.x, pt.y - bbox.y));
        }
        fillPoly(mask, vector<vector<Point>>{poly_relative}, Scalar(255));
        
        // Fast scanline sum using pointer arithmetic
        for (int y = 0; y < bbox.height; y++) {
            const uchar* mask_row = mask.ptr<uchar>(y);
            const Vec3b* img_row = img.ptr<Vec3b>(bbox.y + y) + bbox.x;
            
            for (int x = 0; x < bbox.width; x++) {
                if (mask_row[x]) {
                    const Vec3b& pixel = img_row[x];
                    sum_b += pixel[0];
                    sum_g += pixel[1];
                    sum_r += pixel[2];
                    pixel_count++;
                }
            }
        }
        
        if (pixel_count > 0) {
            dominant_colors[idx] = Vec3b(
                (uchar)(sum_b / pixel_count),
                (uchar)(sum_g / pixel_count),
                (uchar)(sum_r / pixel_count)
            );
        } else {
            dominant_colors[idx] = Vec3b(0, 0, 0);
        }
    }
    
    auto calc_end = high_resolution_clock::now();
    cout << "  Color calculation: " << duration_cast<milliseconds>(calc_end - calc_start).count() << " ms" << endl;
    
    auto total_end = high_resolution_clock::now();
    auto total_duration = duration_cast<milliseconds>(total_end - total_start);
    cout << "Dominant color calculation completed in " << total_duration.count() << " ms (full resolution)" << endl;
    
    cout << "BGR dominant per cell (row-major): ";
    for (const auto& color : dominant_colors) {
        cout << "(" << (int)color[0] << "," << (int)color[1] << "," << (int)color[2] << ") ";
    }
    cout << endl;
    
    // Create dominant color grid visualization
    int grid_cell_width = config.grid_cell_width;
    int grid_cell_height = config.grid_cell_height;
    int grid_width = cols * grid_cell_width;
    int grid_height = rows * grid_cell_height;
    
    Mat color_grid = Mat::zeros(grid_height, grid_width, CV_8UC3);
    
    for (int r = 0; r < rows; r++) {
        for (int c = 0; c < cols; c++) {
            int color_index = r * cols + c;
            Vec3b color = dominant_colors[color_index];
            
            // Calculate cell position in the grid
            int x_start = c * grid_cell_width;
            int y_start = r * grid_cell_height;
            int x_end = x_start + grid_cell_width;
            int y_end = y_start + grid_cell_height;
            
            // Fill the cell with the dominant color
            rectangle(color_grid, Point(x_start, y_start), Point(x_end, y_end), 
                     Scalar(color[0], color[1], color[2]), -1);
            
            // Add a thin border for better visibility
            rectangle(color_grid, Point(x_start, y_start), Point(x_end, y_end), 
                     Scalar(255, 255, 255), config.border_thickness);
            
            // Add text showing the cell coordinates
            if (config.show_coordinates) {
                string coord_text = to_string(r) + "," + to_string(c);
                putText(color_grid, coord_text, Point(x_start + 2, y_start + 15), 
                       FONT_HERSHEY_SIMPLEX, config.coordinate_font_scale, Scalar(255, 255, 255), 1);
            }
        }
    }
    
    // Save the color grid
    string color_grid_path = output_dir + "/dominant_color_grid.png";
    imwrite(color_grid_path, color_grid);
    cout << "Dominant color grid saved as: " << color_grid_path << endl;
   
    return 0;
}

