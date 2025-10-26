# Quick Start Guide

## 5-Minute Setup

### 1. Install Dependencies

**macOS:**
```bash
brew install cmake opencv nlohmann-json libomp
```

**Raspberry Pi / Linux:**
```bash
sudo apt update
sudo apt install build-essential cmake libopencv-dev nlohmann-json3-dev
```

### 2. Build

```bash
cd /path/to/cpp
mkdir build && cd build
cmake ..
make -j$(nproc)
```

### 3. Test with Demo Image

```bash
cd ..
./build/bin/app --debug --single-frame --save-debug
```

**Expected output:**
```
[INFO] === LED Controller Starting ===
[INFO] Loaded image: img2.png (2566x980)
[INFO] Frame processed in ~100 ms
[INFO] Saved debug boundaries to output/debug_boundaries.png
[INFO] Saved color grid to output/dominant_color_grid.png
```

Check the `output/` directory for generated images!

## Common Use Cases

### Debug Mode (Test with Static Image)

```bash
# Process single frame
./build/bin/app --debug --image my_test.png --single-frame --save-debug

# With verbose logging
./build/bin/app --debug --single-frame --save-debug --verbose
```

### Live Camera Mode (Raspberry Pi 5)

```bash
# Continuous processing
./build/bin/app --live --camera /dev/video0

# With target FPS
# (edit config.json: "target_fps": 30)
./build/bin/app --live
```

### With HyperHDR

1. Start HyperHDR on your system
2. Edit `config.json`:
```json
{
  "mode": "live",
  "hyperhdr": {
    "enabled": true,
    "host": "127.0.0.1",
    "port": 19400
  }
}
```
3. Run:
```bash
./build/bin/app --live
```

## Configuration

The easiest way to configure is to edit `config.json`:

### Change Grid Size
```json
"led_layout": {
  "format": "grid",
  "grid": {
    "rows": 10,    // Change this
    "cols": 16     // Change this
  }
}
```

### Adjust Performance
```json
"performance": {
  "target_fps": 60,                    // 0 = max speed
  "enable_parallel_processing": true   // Use OpenMP
}
```

### Camera Settings
```json
"camera": {
  "device": "/dev/video0",
  "width": 1920,
  "height": 1080,
  "fps": 30
}
```

## Troubleshooting

### "Could not read image"
- Check the image path in `config.json` or `--image` parameter
- Make sure the image exists and is readable

### "Failed to connect to HyperHDR"
- Verify HyperHDR is running: `netstat -an | grep 19400`
- Check host/port in config.json
- Try with `--debug` mode first to test without HyperHDR

### Camera not working
- Run: `ls -l /dev/video*` to check camera devices
- On Raspberry Pi: `libcamera-hello` to test camera
- Note: Camera support requires completion on actual Pi 5 hardware

### Build fails
- Check all dependencies are installed
- Try: `cmake .. -DCMAKE_VERBOSE_MAKEFILE=ON` for detailed output
- macOS: Ensure Xcode Command Line Tools installed

## Next Steps

1. **Customize Bézier curves** for your TV shape
2. **Calibrate LED positions** in config.json
3. **Test with HyperHDR** for real LED output
4. **Optimize performance** on your target hardware
5. **Set up autostart** (systemd on Linux/Pi)

## More Information

- See **README.md** for full documentation
- See **IMPLEMENTATION_STATUS.md** for feature status
- See **config.hyperhdr.json** for HyperHDR example
- Check **output/** directory for debug visualizations

## Getting Help

Common commands:
```bash
./build/bin/app --help           # Show all options
./build/bin/app --verbose        # Enable debug logging
./build/bin/app_legacy           # Run original version for comparison
```

Happy LED controlling! 🎨✨

