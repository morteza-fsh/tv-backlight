#pragma once

#include <opencv2/opencv.hpp>
#include <vector>
#include <string>

namespace TVLED {

class BezierCurve {
public:
    BezierCurve() = default;
    
    // Parse a single cubic Bézier curve from SVG path format
    bool parse(const std::string& bezierPath, int numSamples = 50);
    
    // Get the sampled points
    const std::vector<cv::Point2f>& getPoints() const { return points_; }
    
    // Scale all points
    void scale(float factor);
    
    // Translate all points
    void translate(float offsetX, float offsetY);
    
    // Clamp all points to boundaries
    void clamp(float minX, float maxX, float minY, float maxY);
    
    // Get start and end points
    cv::Point2f getStart() const { return points_.empty() ? cv::Point2f(0, 0) : points_.front(); }
    cv::Point2f getEnd() const { return points_.empty() ? cv::Point2f(0, 0) : points_.back(); }
    
    size_t size() const { return points_.size(); }
    bool empty() const { return points_.empty(); }

private:
    std::vector<cv::Point2f> points_;
};

} // namespace TVLED

