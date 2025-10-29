# Modular TV Backlight LED Controller

A high-performance, modular C++ application for calculating TV backlight LED colors using Coons patch interpolation and Bézier curves. Designed for Raspberry Pi 5 with native libcamera CSI camera support and HyperHDR integration.

## 🎉 NEW: Simple & Fast Camera via Pipe!

The camera code now uses the **simplest possible approach** - just pipes from `rpicam-vid`:

- ✅ **Dead simple** - just `popen()` and `fread()`
- ✅ **Lowest latency** - direct hardware stream (~20ms/frame)
- ✅ **No complex libraries** - just rpicam-apps (standard on Pi OS)
- ✅ **No legacy mode needed!**

**Quick Start on Raspberry Pi:**
```bash
./setup_simple.sh  # Installs rpicam-apps, OpenCV, builds
./build/bin/app --live --camera 0 --single-frame --verbose
```

📖 **Quick setup:** `./setup_simple.sh` then `./build/bin/app --live --camera 0 --single-frame --verbose`

## Features

- ✅ **Modular Architecture**: Clean separation of concerns with dedicated modules for each task
- ✅ **Dual Mode Operation**: Debug mode (static images) and Live mode (camera input)
- ✅ **Coons Patch Interpolation**: Advanced curved screen mapping using Bézier curves
- ✅ **HyperHDR Integration**: Flatbuffer protocol support for LED communication
- ✅ **High Performance**: OpenMP parallelization and optimized color extraction
- ✅ **Flexible LED Layouts**: Support for both grid and HyperHDR edge-based layouts
- ✅ **Raspberry Pi 5 Ready**: Simple pipe-based camera with ultra-low latency (~20ms/frame)
- ✅ **Configurable FPS**: Target frame rate control or maximum speed mode
- ✅ **Debug Visualization**: Save boundary curves and color grids for debugging

## Architecture

```
src/
├── main.cpp                          # Entry point with CLI argument parsing
├── core/
│   ├── Config.h/cpp                  # Configuration management
│   ├── FrameSource.h                 # Abstract frame source interface
│   ├── ImageFrameSource.h/cpp        # Debug mode: static image input
│   ├── CameraFrameSource.h/cpp       # Live mode: simple rpicam-vid pipe
│   └── LEDController.h/cpp           # Main orchestrator
├── processing/
│   ├── BezierCurve.h/cpp            # Bézier curve parsing & sampling
│   ├── CoonsPatching.h/cpp          # Coons patch interpolation
│   └── ColorExtractor.h/cpp         # Dominant color calculation
├── communication/
│   ├── HyperHDRClient.h/cpp         # Flatbuffer protocol implementation
│   └── LEDLayout.h/cpp              # LED layout configuration & conversion
└── utils/
    ├── PerformanceTimer.h           # Profiling utilities
    └── Logger.h                     # Logging system
```

## Prerequisites

### macOS
- Xcode Command Line Tools: `xcode-select --install`
- CMake: `brew install cmake`
- OpenCV: `brew install opencv`
- nlohmann-json: `brew install nlohmann-json`
- OpenMP: `brew install libomp`

### Raspberry Pi / Linux

**Automated Setup (Recommended):**
```bash
./setup_simple.sh  # Installs rpicam-apps, OpenCV, and builds
```

**Manual Setup:**
- Build tools: `sudo apt install build-essential cmake`
- **rpicam-apps** (camera utilities): `sudo apt install rpicam-apps`
- OpenCV: `sudo apt install libopencv-dev`
- nlohmann-json: `sudo apt install nlohmann-json3-dev` (or will be fetched automatically)

**That's it!** No complex camera libraries needed.

## Building

```bash
# Configure
mkdir build && cd build
cmake ..

# Build
make -j$(nproc)

# Output binaries
./bin/app          # Modular version
```

## Usage

### Command Line Options

```bash
./build/bin/app [OPTIONS]

Options:
  --config <path>      Path to config file (default: config.json)
  --debug              Run in debug mode (static image)
  --live               Run in live mode (camera)
  --image <path>       Input image for debug mode
  --camera <device>    Camera device (default: /dev/video0)
  --single-frame       Process single frame and exit
  --save-debug         Save debug images
  --verbose            Enable verbose logging
  --help               Show this help message
```

### Examples

#### Debug Mode with Single Frame
```bash
# Process a single image and save debug outputs
./build/bin/app --debug --image img2.png --single-frame --save-debug
```

#### Live Camera Mode (Raspberry Pi 5)
```bash
# Continuous processing from camera
./build/bin/app --live --camera /dev/video0
```

#### With HyperHDR Integration
```bash
# Edit config.json to enable HyperHDR
./build/bin/app --live
```

## Configuration

The `config.json` file controls all aspects of the application:

```json
{
  "mode": "debug",                    // "debug" or "live"
  "input_image": "img2.png",          // Image for debug mode
  "output_directory": "output",        // Where to save debug images
  
  "camera": {
    "device": "/dev/video0",          // Camera device path
    "width": 1920,                    // Capture resolution
    "height": 1080,
    "fps": 30                         // Capture framerate
  },
  
  "hyperhdr": {
    "enabled": false,                 // Enable HyperHDR communication
    "host": "127.0.0.1",             // HyperHDR server address
    "port": 19400,                    // HyperHDR server port
    "priority": 100                   // Priority level
  },
  
  "led_layout": {
    "format": "grid",                 // "grid" or "hyperhdr"
    "grid": {
      "rows": 5,                      // Grid rows
      "cols": 8                       // Grid columns
    },
    "hyperhdr": {                     // Alternative: edge-based layout
      "top": 20,
      "bottom": 20,
      "left": 10,
      "right": 10
    }
  },
  
  "bezier_curves": {                  // SVG path format Bézier curves
    "left_bezier": "M 4.5 208 C ...",
    "bottom_bezier": "M 4.5 208 C ...",
    "right_bezier": "M 917.5 9 C ...",
    "top_bezier": "M 325 22.9999 C ..."
  },
  
  "bezier_settings": {
    "use_direct_bezier_curves": true,
    "bezier_samples": 50,             // Points to sample on curve
    "polygon_samples": 15             // Points for cell polygons
  },
  
  "scaling": {
    "scale_factor": 2.0               // Scale curves to fit image
  },
  
  "visualization": {                  // Debug output settings
    "grid_cell_width": 60,
    "grid_cell_height": 40,
    "debug_boundary_thickness": 3,
    "debug_corner_radius": 10
  },
  
  "color_settings": {
    "show_coordinates": true,
    "coordinate_font_scale": 0.4,
    "border_thickness": 1
  },
  
  "performance": {
    "target_fps": 0,                  // 0 = maximum speed
    "enable_parallel_processing": true,
    "parallel_chunk_size": 4
  }
}
```

## Module Documentation

### Core Modules

**Config** - Configuration management
- Loads and validates JSON configuration
- Supports command-line overrides
- Backward compatible with old config format

**FrameSource** - Abstract frame source interface
- `ImageFrameSource`: Loads static images for debugging
- `CameraFrameSource`: Captures from libcamera (placeholder for Pi 5)

**LEDController** - Main orchestrator
- Chains all modules together
- Manages processing loop
- Handles FPS throttling and monitoring

### Processing Modules

**BezierCurve** - Bézier curve parsing and sampling
- Parses SVG path format cubic Bézier curves
- Samples curves into point arrays
- Supports scaling, translation, and clamping

**CoonsPatching** - Coons patch interpolation
- Maps rectangular grid to curved boundaries
- Uses cached polylines for fast interpolation
- Pre-computes cell polygons for efficiency

**ColorExtractor** - Dominant color calculation
- Extracts average colors from curved regions
- OpenMP parallelization for performance
- Converts BGR to RGB for HyperHDR

### Communication Modules

**LEDLayout** - LED layout management
- Supports grid format (rows × cols)
- Supports HyperHDR format (edge counts)
- Handles LED ordering and indexing

**HyperHDRClient** - HyperHDR communication
- TCP socket connection
- Flatbuffer protocol serialization (simplified version)
- Automatic reconnection handling

### Utilities

**Logger** - Thread-safe logging
- Multiple log levels (DEBUG, INFO, WARN, ERROR)
- Timestamped output
- Configurable verbosity

**PerformanceTimer** - Performance profiling
- RAII-based timing
- Millisecond and microsecond precision
- Automatic reporting

## Performance

On a typical setup:
- **Debug mode (single frame)**: ~100ms total (1ms polygon generation, 15ms color extraction)
- **Color extraction**: Parallelized with OpenMP for maximum throughput
- **Target**: Optimized for maximum FPS on Raspberry Pi 5

## Output

Debug mode generates:
- `output/debug_boundaries.png`: Visual representation of Bézier curves and grid
- `output/dominant_color_grid.png`: Grid visualization of extracted LED colors

## HyperHDR Integration

The application sends RGB color data to HyperHDR using a simplified Flatbuffer-style protocol:

1. Connect to HyperHDR server (default: `127.0.0.1:19400`)
2. Send color data with priority level
3. Colors are in RGB format (converted from OpenCV's BGR)

**Note**: The current implementation uses a simplified protocol. For full HyperHDR Flatbuffer schema support, the schema files need to be integrated (planned for hardware testing phase).

## Raspberry Pi 5 Deployment

### libcamera Support

The `CameraFrameSource` class is prepared for libcamera integration but requires completion on actual Raspberry Pi 5 hardware:

```cpp
// TODO in src/core/CameraFrameSource.cpp:
// 1. Initialize libcamera::CameraManager
// 2. Configure camera for requested resolution/fps
// 3. Start capture pipeline
// 4. Convert libcamera frames to cv::Mat
```

### Building on Raspberry Pi 5

```bash
# Install dependencies
sudo apt update
sudo apt install build-essential cmake
sudo apt install libopencv-dev nlohmann-json3-dev
sudo apt install libcamera-dev

# Build
mkdir build && cd build
cmake ..
make -j4

# Test with static image first
./bin/app --debug --single-frame --save-debug

# Then try live mode
./bin/app --live --camera /dev/video0
```

## Development

### Adding New Features

1. **New Frame Source**: Implement `FrameSource` interface
2. **New LED Layout**: Extend `LEDLayout` class
3. **New Processing**: Add to `processing/` module

### Testing

```bash
# Test with debug mode (static image)
./build/bin/app --debug --single-frame --save-debug --verbose

# Check output matches
diff output/dominant_color_grid.png output_legacy/dominant_color_grid.png
```

## Troubleshooting

### Build Issues

**nlohmann-json not found**:
```bash
brew install nlohmann-json  # macOS
sudo apt install nlohmann-json3-dev  # Linux
```

**OpenMP not found**:
```bash
brew install libomp  # macOS
```

### Runtime Issues

**Camera not found**:
- Check device permissions: `ls -l /dev/video*`
- Verify libcamera installation: `libcamera-hello`

**HyperHDR connection failed**:
- Check HyperHDR is running: `netstat -an | grep 19400`
- Verify host/port in `config.json`

## Future Enhancements

- [ ] Complete libcamera integration on Raspberry Pi 5
- [ ] Full HyperHDR Flatbuffer schema support
- [ ] Network-based configuration UI
- [ ] Additional LED layout patterns
- [ ] Performance benchmarking suite
- [ ] Automated calibration tools

## License

This project is provided as-is for educational and personal use.

## Credits

- OpenCV for image processing
- nlohmann/json for configuration parsing
- HyperHDR for LED control protocol
- libcamera for Raspberry Pi camera support
