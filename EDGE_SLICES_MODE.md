# Edge Slices Mode Documentation

## Overview

The Edge Slices mode is a new color extraction method optimized for TV backlighting that samples colors only from the edges of the screen (top, bottom, left, right), avoiding the center which is typically not useful for ambient lighting.

## Configuration

Add a `color_extraction` section to your `config.json`:

```json
"color_extraction": {
  "mode": "edge_slices",
  "horizontal_coverage_percent": 20.0,
  "vertical_coverage_percent": 15.0,
  "horizontal_slices": 10,
  "vertical_slices": 8
}
```

### Parameters

- **mode**: `"edge_slices"` or `"grid"`
  - `"edge_slices"`: TV backlight mode - samples only from screen edges
  - `"grid"`: Traditional mode - samples entire screen in a grid pattern

- **horizontal_coverage_percent**: `0.0` to `100.0` (default: `20.0`)
  - Percentage of screen height to sample from top and bottom edges
  - Example: `20.0` means the top 20% and bottom 20% of the screen are sampled

- **vertical_coverage_percent**: `0.0` to `100.0` (default: `20.0`)
  - Percentage of screen width to sample from left and right edges
  - Example: `15.0` means the leftmost 15% and rightmost 15% are sampled

- **horizontal_slices**: Integer > 0 (default: `10`)
  - Number of LED segments along the top and bottom edges
  - Total horizontal LEDs = 2 × horizontal_slices

- **vertical_slices**: Integer > 0 (default: `8`)
  - Number of LED segments along the left and right edges (excluding corners)
  - Total vertical LEDs = 2 × vertical_slices

### Total LED Count

When using edge_slices mode:
```
Total LEDs = (2 × horizontal_slices) + (2 × vertical_slices)
```

Example with default values:
```
Total LEDs = (2 × 10) + (2 × 8) = 36 LEDs
```

## How It Works

1. **Bezier Curves**: The system still uses bezier curves to define the screen boundary, allowing for curved or irregular screen shapes.

2. **Edge Regions**:
   - **Top Edge**: Divided into `horizontal_slices` segments, covering the top `horizontal_coverage_percent` of the screen height, full width
   - **Bottom Edge**: Divided into `horizontal_slices` segments, covering the bottom `horizontal_coverage_percent` of the screen height, full width
   - **Left Edge**: Divided into `vertical_slices` segments, covering the left `vertical_coverage_percent` of the screen width, **full height** (overlaps with corners)
   - **Right Edge**: Divided into `vertical_slices` segments, covering the right `vertical_coverage_percent` of the screen width, **full height** (overlaps with corners)

3. **Color Extraction**: For each edge segment, the dominant color is calculated by averaging all pixels within that region.

4. **LED Order**: Colors are returned in order:
   - LEDs 0 to (horizontal_slices-1): Top edge, left to right
   - LEDs horizontal_slices to (2×horizontal_slices-1): Bottom edge, left to right
   - LEDs 2×horizontal_slices to (2×horizontal_slices+vertical_slices-1): Left edge, top to bottom (full height, includes corner overlap)
   - LEDs (2×horizontal_slices+vertical_slices) to end: Right edge, top to bottom (full height, includes corner overlap)

## Benefits for TV Backlighting

1. **Focused Sampling**: Only samples from edges where backlights are typically placed
2. **Excludes Center**: Ignores the center of the screen which doesn't contribute to ambient lighting
3. **Full Corner Coverage**: Left and right edges span full height with corner overlap, ensuring no gaps in LED coverage
4. **Efficient**: Fewer LEDs to process compared to full grid mode
5. **Natural Lighting**: Better represents the actual TV backlight placement
6. **Configurable Coverage**: Adjust how much of the edge to sample based on your LED strip placement

## Usage Examples

### Typical TV Backlight Setup
```json
"color_extraction": {
  "mode": "edge_slices",
  "horizontal_coverage_percent": 20.0,
  "vertical_coverage_percent": 15.0,
  "horizontal_slices": 20,
  "vertical_slices": 10
}
```
This creates 60 LEDs total, perfect for a typical LED strip setup.

### Narrow Edge Sampling
```json
"color_extraction": {
  "mode": "edge_slices",
  "horizontal_coverage_percent": 10.0,
  "vertical_coverage_percent": 10.0,
  "horizontal_slices": 15,
  "vertical_slices": 8
}
```
This samples only the outermost 10% of the screen, ideal for very edge-focused lighting.

### Wide Coverage
```json
"color_extraction": {
  "mode": "edge_slices",
  "horizontal_coverage_percent": 30.0,
  "vertical_coverage_percent": 25.0,
  "horizontal_slices": 12,
  "vertical_slices": 10
}
```
This samples a larger portion of the edges for more content-aware lighting.

## Switching Between Modes

You can easily switch between edge_slices and grid modes by changing the `mode` parameter:

```json
"color_extraction": {
  "mode": "grid"  // Use traditional grid mode
}
```

The grid mode will use the `led_layout` configuration instead.

## Debug Visualization

When running with `--save-debug` flag, two images are generated:

1. **debug_boundaries.png**: Shows the bezier curves and edge slice regions overlaid on the input image
2. **dominant_color_grid.png**: Shows the extracted colors in a linear strip format with LED indices

## Implementation Details

- Edge slice polygons are generated dynamically using Coons patching interpolation
- The same parallel processing optimizations apply as in grid mode
- Bezier curves are still required to define the screen boundary
- Compatible with HyperHDR output

