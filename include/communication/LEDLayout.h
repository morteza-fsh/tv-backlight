#pragma once

#include <vector>
#include <string>

namespace TVLED {

enum class LEDLayoutFormat {
    GRID,      // Rows x Cols grid
    HYPERHDR   // Top, Bottom, Left, Right edge counts
};

class LEDLayout {
public:
    LEDLayout() = default;
    
    // Create from grid format (rows x cols)
    static LEDLayout fromGrid(int rows, int cols);
    
    // Create from HyperHDR format (edge counts)
    static LEDLayout fromHyperHDR(int top, int bottom, int left, int right);
    
    // Get total number of LEDs
    int getTotalLEDs() const;
    
    // Get LED ordering (starts from bottom-left, goes clockwise)
    // Returns indices in order: left (B->T), top (L->R), right (T->B), bottom (R->L)
    // Note: left and bottom arrays are reversed when extracting colors
    std::vector<int> getLEDOrder() const;
    
    // Convert grid coordinates to LED index
    int gridToLEDIndex(int row, int col) const;
    
    // Get layout format
    LEDLayoutFormat getFormat() const { return format_; }
    
    // Get grid dimensions (if applicable)
    int getRows() const { return rows_; }
    int getCols() const { return cols_; }
    
    // Get edge counts (if applicable)
    int getTopCount() const { return top_count_; }
    int getBottomCount() const { return bottom_count_; }
    int getLeftCount() const { return left_count_; }
    int getRightCount() const { return right_count_; }

private:
    LEDLayoutFormat format_;
    
    // Grid format
    int rows_;
    int cols_;
    
    // HyperHDR format
    int top_count_;
    int bottom_count_;
    int left_count_;
    int right_count_;
    
    void computeLEDOrder();
    std::vector<int> led_order_;
};

} // namespace TVLED

