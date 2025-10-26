# Camera Setup Guide for Raspberry Pi

## Changes Made to Fix Camera Issues

### Summary
The C++ code now matches the behavior of the working Python `picamera2` script:
- **Increased warmup time**: Now waits ~2 seconds (like Python's `time.sleep(2)`)
- **Better error handling**: More informative error messages
- **Explicit V4L2 backend**: Tries V4L2 first (most compatible with Raspberry Pi)
- **Proper stabilization**: Continues reading frames during warmup to allow auto-exposure/white balance to settle

### Key Improvements in `CameraFrameSource.cpp`

1. **Backend Selection** (lines 50-75):
   - Tries V4L2 backend explicitly first
   - Falls back to any available backend
   - Provides helpful error messages about Raspberry Pi camera setup

2. **Extended Warmup** (lines 115-157):
   - Initial 500ms delay for camera wake-up
   - 20 attempts over 2 seconds to get first valid frame
   - Additional 1.5 seconds of continuous frame reading for stabilization
   - Total ~2-2.5 seconds warmup (matching Python behavior)

## Raspberry Pi Camera Module Setup

### If using Raspberry Pi Camera Module (not USB camera)

The Raspberry Pi Camera Module doesn't create a `/dev/video0` device by default. You have two options:

#### Option 1: Enable Legacy Camera Support (Recommended for OpenCV)

```bash
sudo raspi-config
# Navigate to: Interface Options -> Legacy Camera -> Enable
# Reboot
sudo reboot
```

After reboot, check for the video device:
```bash
ls -l /dev/video*
```

You should see `/dev/video0`.

#### Option 2: Use libcamera-vid to create V4L2 device

```bash
# Install v4l2loopback
sudo modprobe v4l2loopback

# Stream camera through v4l2loopback
libcamera-vid -t 0 --codec yuv420 -o - | \
  ffmpeg -f rawvideo -pix_fmt yuv420p -s 1920x1080 -i - \
  -f v4l2 /dev/video0
```

This is more complex and not ideal for this application.

### For USB Cameras

USB cameras should work out of the box. List available devices:
```bash
v4l2-ctl --list-devices
```

## Testing the Fixed Code

### On Raspberry Pi

```bash
# Build the project (if not already built)
cd /Users/morteza/Projects/Playground/TV/cpp
cmake --preset debug-opencv
cmake --build build/debug-opencv -j4

# Test with camera (verbose mode to see all debug info)
./build/debug-opencv/bin/app --live --camera /dev/video0 --single-frame --verbose

# Or test with saved image first to verify rest of pipeline
./build/debug-opencv/bin/app --debug --image img.png --single-frame --save-debug --verbose
```

### Expected Output (Success)

```
[INFO ] Camera opened with V4L2 backend
[DEBUG] Warming up camera (allowing auto-exposure/white balance to settle)...
[DEBUG] Got first valid frame on attempt 3
[DEBUG] Camera responding, continuing warmup for stabilization...
[INFO ] Camera warmup complete, read 28 stabilization frames
[INFO ] Frame source ready: CameraFrameSource: /dev/video0 (1920x1080@30fps)
```

## Troubleshooting

### Issue: "Failed to open camera device"

**Solution**: Check that the device exists:
```bash
ls -l /dev/video*
v4l2-ctl --list-devices
```

If no device exists and you have Pi Camera Module, enable legacy camera support (see above).

### Issue: "Camera opened but read() returns false"

**Causes**:
1. Camera is being used by another process
2. Insufficient permissions
3. Wrong resolution/fps combination

**Solutions**:
```bash
# Check if camera is in use
sudo fuser /dev/video0

# Give your user video group access
sudo usermod -a -G video $USER

# Try different resolution in config.json
{
  "camera": {
    "device": "/dev/video0",
    "width": 640,    // Try lower resolution first
    "height": 480,
    "fps": 30
  }
}
```

### Issue: Frames read during warmup but fail afterward

This suggests the camera needs even more stabilization time. Try increasing the warmup delay in the code.

## Differences: picamera2 vs OpenCV

| Feature | picamera2 (Python) | OpenCV (C++) |
|---------|-------------------|--------------|
| Backend | Native libcamera | V4L2 wrapper |
| Setup | Camera-specific config | Generic video capture |
| Performance | Better (direct access) | Good (through V4L2) |
| Portability | Raspberry Pi only | Works anywhere |
| Warmup | 2 seconds (explicit) | Now 2+ seconds |

## Performance Considerations

The 2-second warmup only happens once at startup. After that, the camera runs at full speed (30 fps or whatever is configured).

## Next Steps

1. **Test with verbose logging** to see exactly what's happening
2. **Verify camera device exists** (`ls /dev/video*`)
3. **Check config.json** camera settings match your hardware
4. **Try lower resolution** if having issues (640x480 instead of 1920x1080)
5. **Monitor system resources** - Raspberry Pi can struggle with high-res processing

## Config Reference

Your `config.json` camera section should look like:
```json
{
  "camera": {
    "device": "/dev/video0",
    "width": 1920,
    "height": 1080,
    "fps": 30
  }
}
```

For Raspberry Pi 3 or earlier, consider:
```json
{
  "camera": {
    "device": "/dev/video0",
    "width": 1280,
    "height": 720,
    "fps": 30
  }
}
```

