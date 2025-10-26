#include "processing/BezierCurve.h"
#include "utils/Logger.h"
#include <regex>
#include <cmath>
#include <algorithm>

namespace TVLED {

bool BezierCurve::parse(const std::string& bezierPath, int numSamples) {
    points_.clear();
    
    try {
        // Extract start point (M command)
        std::regex move_regex(R"(M\s*([\d.-]+)\s+([\d.-]+))");
        std::smatch move_match;
        if (!std::regex_search(bezierPath, move_match, move_regex)) {
            LOG_ERROR("Invalid Bézier curve format: M command not found");
            return false;
        }
        
        float start_x = std::stof(move_match[1].str());
        float start_y = std::stof(move_match[2].str());
        
        // Extract control points and end point (C command)
        std::regex curve_regex(R"(C\s*([\d.-]+)\s+([\d.-]+)\s+([\d.-]+)\s+([\d.-]+)\s+([\d.-]+)\s+([\d.-]+))");
        std::smatch curve_match;
        if (!std::regex_search(bezierPath, curve_match, curve_regex)) {
            LOG_ERROR("Invalid Bézier curve format: C command not found");
            return false;
        }
        
        float x1 = std::stof(curve_match[1].str());
        float y1 = std::stof(curve_match[2].str());
        float x2 = std::stof(curve_match[3].str());
        float y2 = std::stof(curve_match[4].str());
        float x3 = std::stof(curve_match[5].str());
        float y3 = std::stof(curve_match[6].str());
        
        // Sample points along the cubic bezier curve
        points_.reserve(numSamples);
        for (int i = 0; i < numSamples; i++) {
            float t = static_cast<float>(i) / (numSamples - 1);
            // Cubic bezier formula: B(t) = (1-t)³P0 + 3(1-t)²tP1 + 3(1-t)t²P2 + t³P3
            float x = std::pow(1-t, 3) * start_x + 3*std::pow(1-t, 2)*t * x1 + 
                      3*(1-t)*std::pow(t, 2) * x2 + std::pow(t, 3) * x3;
            float y = std::pow(1-t, 3) * start_y + 3*std::pow(1-t, 2)*t * y1 + 
                      3*(1-t)*std::pow(t, 2) * y2 + std::pow(t, 3) * y3;
            points_.push_back(cv::Point2f(x, y));
        }
        
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR(std::string("Error parsing Bézier curve: ") + e.what());
        points_.clear();
        return false;
    }
}

void BezierCurve::scale(float factor) {
    for (auto& pt : points_) {
        pt *= factor;
    }
}

void BezierCurve::translate(float offsetX, float offsetY) {
    for (auto& pt : points_) {
        pt.x += offsetX;
        pt.y += offsetY;
    }
}

void BezierCurve::clamp(float minX, float maxX, float minY, float maxY) {
    for (auto& pt : points_) {
        pt.x = std::clamp(pt.x, minX, maxX);
        pt.y = std::clamp(pt.y, minY, maxY);
    }
}

} // namespace TVLED

