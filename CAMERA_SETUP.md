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

### The Core Issue

**Your Python `picamera2` code works because it uses the modern `libcamera` API.**  
**OpenCV uses V4L2, which doesn't work with Pi Camera Module on modern Raspberry Pi OS by default.**

### Solution: Enable Legacy Camera Support (REQUIRED for OpenCV)

On modern Raspberry Pi OS, the camera module uses libcamera only. OpenCV needs V4L2 support, which requires enabling legacy camera mode:

```bash
# Enable legacy camera support
sudo raspi-config
# Navigate to: Interface Options -> Legacy Camera -> Enable
# Reboot
sudo reboot
```

**After reboot**, check for the video device:
```bash
ls -l /dev/video*
# Should show: /dev/video0

# Verify it works:
v4l2-ctl -d /dev/video0 --list-formats-ext
```

### Why This Is Necessary

| API | Python picamera2 | OpenCV C++ | 
|-----|------------------|------------|
| Interface | libcamera (native) | V4L2 only |
| Pi Camera Module | ✅ Works directly | ❌ Needs legacy mode |
| USB Camera | N/A | ✅ Works directly |

**Without legacy mode enabled**: `/dev/video0` may exist but OpenCV can't read frames from it (exactly your error).

### Alternative: Use a USB Webcam

If you can't or don't want to enable legacy mode, use a USB webcam instead. USB cameras work directly with V4L2/OpenCV.

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

### Issue: "Camera opened with V4L2 backend" but "Failed to read any frames"

**This is THE most common issue.** The camera device exists, but OpenCV can't read from it.

**Root Cause**: Raspberry Pi Camera Module on modern Raspberry Pi OS uses libcamera, not V4L2.

**Solution**: Enable legacy camera support:
```bash
sudo raspi-config
# Interface Options -> Legacy Camera -> Enable -> Yes
sudo reboot

# After reboot, test:
v4l2-ctl -d /dev/video0 --list-formats-ext
# Should show available formats like MJPEG, YUYV, etc.
```

### Issue: "Unable to get camera FPS" warning

This warning appears when V4L2 can't query the camera properly. Usually accompanies the "can't read frames" issue.

**Solution**: Same as above - enable legacy camera support.

### Issue: "Failed to open camera device"

**Solution**: Check that the device exists:
```bash
ls -l /dev/video*
v4l2-ctl --list-devices
```

If no `/dev/video0` exists:
- **Pi Camera Module**: Enable legacy camera (see above)
- **USB Camera**: Check `lsusb` to see if it's detected

### Issue: Camera works but images are wrong/corrupted

**Causes**:
1. Wrong resolution/format combination
2. Insufficient USB power (for USB cameras)
3. Camera needs more warmup time

**Solutions**:
```bash
# Check supported formats:
v4l2-ctl -d /dev/video0 --list-formats-ext

# Try different resolution in config.json:
{
  "camera": {
    "device": "/dev/video0",
    "width": 640,    // Start with lower resolution
    "height": 480,
    "fps": 30
  }
}

# For USB cameras, check power:
dmesg | grep -i usb
# Look for power-related warnings
```

### Issue: "Camera is being used by another process"

```bash
# Check what's using the camera
sudo fuser /dev/video0

# Kill the process if needed
sudo fuser -k /dev/video0
```

### Issue: Permission denied

```bash
# Check permissions
ls -l /dev/video0

# Add your user to video group
sudo usermod -a -G video $USER

# Log out and back in, or reboot
sudo reboot
```

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

