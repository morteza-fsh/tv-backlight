#include "communication/LEDLayout.h"
#include "utils/Logger.h"

namespace TVLED {

LEDLayout LEDLayout::fromGrid(int rows, int cols) {
    LEDLayout layout;
    layout.format_ = LEDLayoutFormat::GRID;
    layout.rows_ = rows;
    layout.cols_ = cols;
    layout.top_count_ = 0;
    layout.bottom_count_ = 0;
    layout.left_count_ = 0;
    layout.right_count_ = 0;
    layout.computeLEDOrder();
    
    LOG_INFO("Created grid LED layout: " + std::to_string(rows) + "x" + std::to_string(cols) +
             " (" + std::to_string(layout.getTotalLEDs()) + " LEDs)");
    
    return layout;
}

LEDLayout LEDLayout::fromHyperHDR(int top, int bottom, int left, int right) {
    LEDLayout layout;
    layout.format_ = LEDLayoutFormat::HYPERHDR;
    layout.rows_ = 0;
    layout.cols_ = 0;
    layout.top_count_ = top;
    layout.bottom_count_ = bottom;
    layout.left_count_ = left;
    layout.right_count_ = right;
    layout.computeLEDOrder();
    
    LOG_INFO("Created HyperHDR LED layout: T=" + std::to_string(top) + 
             " B=" + std::to_string(bottom) + 
             " L=" + std::to_string(left) + 
             " R=" + std::to_string(right) +
             " (" + std::to_string(layout.getTotalLEDs()) + " LEDs)");
    
    return layout;
}

int LEDLayout::getTotalLEDs() const {
    if (format_ == LEDLayoutFormat::GRID) {
        return rows_ * cols_;
    } else {
        return top_count_ + bottom_count_ + left_count_ + right_count_;
    }
}

void LEDLayout::computeLEDOrder() {
    led_order_.clear();
    int total = getTotalLEDs();
    led_order_.reserve(total);
    
    if (format_ == LEDLayoutFormat::GRID) {
        // Simple row-major order for grid
        for (int i = 0; i < total; i++) {
            led_order_.push_back(i);
        }
    } else {
        // HyperHDR clockwise order: top (L->R), right (T->B), bottom (R->L), left (B->T)
        int idx = 0;
        
        // Top edge (left to right)
        for (int i = 0; i < top_count_; i++) {
            led_order_.push_back(idx++);
        }
        
        // Right edge (top to bottom)
        for (int i = 0; i < right_count_; i++) {
            led_order_.push_back(idx++);
        }
        
        // Bottom edge (right to left)
        for (int i = 0; i < bottom_count_; i++) {
            led_order_.push_back(idx++);
        }
        
        // Left edge (bottom to top)
        for (int i = 0; i < left_count_; i++) {
            led_order_.push_back(idx++);
        }
    }
}

std::vector<int> LEDLayout::getLEDOrder() const {
    return led_order_;
}

int LEDLayout::gridToLEDIndex(int row, int col) const {
    if (format_ != LEDLayoutFormat::GRID) {
        LOG_WARN("gridToLEDIndex called on non-grid layout");
        return -1;
    }
    
    if (row < 0 || row >= rows_ || col < 0 || col >= cols_) {
        LOG_WARN("Grid coordinates out of bounds");
        return -1;
    }
    
    return row * cols_ + col;
}

} // namespace TVLED

