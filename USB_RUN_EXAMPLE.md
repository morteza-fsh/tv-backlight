# USB Direct Mode - Run Examples

## Quick Start

### Option 1: Using the Run Script (Easiest)

```bash
# Make sure you're in the project root
cd /Users/morteza/Projects/Playground/TV/cpp

# Run the script
./run_usb_direct.sh
```

The script will:
- ✓ Check if the application is built
- ✓ Verify USB device exists
- ✓ Check permissions
- ✓ Display configuration
- ✓ Calculate expected performance
- ✓ Start the application

### Option 2: Manual Command

```bash
cd /Users/morteza/Projects/Playground/TV/cpp/build
./bin/app --config ../config.hyperhdr.json --live --verbose
```

### Option 3: With Specific Camera Device

```bash
cd /Users/morteza/Projects/Playground/TV/cpp/build
./bin/app \
    --config ../config.hyperhdr.json \
    --live \
    --camera /dev/video0 \
    --verbose
```

## Configuration Summary

**Current Setup:**
- **LED Count:** 216 LEDs (69 top + 69 bottom + 39 left + 39 right)
- **USB Device:** `/dev/ttyUSB0`
- **Baudrate:** 460800 (optimized for 216 LEDs @ 60 FPS)
- **Packet Size:** 654 bytes
- **Max FPS:** ~70 FPS

**Performance Calculation:**
```
Packet Size = 6 + (216 × 3) = 654 bytes
Max FPS = 460800 / (654 × 10) ≈ 70 FPS
```

This gives you comfortable headroom for your 60 FPS target!

## Before Running

### 1. Upload Arduino Sketch

Make sure you've uploaded the receiver sketch to your Arduino/ESP32:

```bash
# Open Arduino IDE
# File → Open → examples/arduino_usb_receiver.ino
# Configure:
#   - LED_PIN: Your LED data pin (default: 6)
#   - MAX_LEDS: 300 (or higher)
#   - SERIAL_BAUD: 460800 (must match config!)
# Upload to your board
```

### 2. Connect Hardware

```
Raspberry Pi ─USB─ Arduino ─Pin 6─ LED Strip Data
                                            
Power Supply 5V ─────────────────────────── LED Strip VCC
Power Supply GND ───┬──────────────────────  LED Strip GND
                    └──────────────────────  Arduino GND
```

### 3. Set Permissions (Linux/Raspberry Pi)

```bash
# Option A: Add user to dialout group (permanent)
sudo usermod -a -G dialout $USER
sudo reboot

# Option B: Temporary permission (lost on reboot)
sudo chmod 666 /dev/ttyUSB0
```

### 4. Verify USB Device

```bash
# Find your USB device
ls -l /dev/ttyUSB* /dev/ttyACM*

# Test serial communication (optional)
stty -F /dev/ttyUSB0 460800
echo "test" > /dev/ttyUSB0
```

## Expected Output

When running successfully, you should see:

```
=== LED Controller Starting ===
Loading configuration from config.hyperhdr.json
Configuration loaded successfully
Initializing LED Controller...
Setting up frame source...
Frame source ready: Camera (rpicam-vid pipe)
Setting up color extractor...
Color extractor ready
Setting up LED layout...
Setting up USB controller...
Opening USB serial device: /dev/ttyUSB0
USB serial device opened successfully: /dev/ttyUSB0 @ 460800 baud
USB controller ready at /dev/ttyUSB0 @ 460800 baud
LED Controller initialized successfully
Starting main processing loop...
Press Ctrl+C to stop
Processing frame: 820x616
Sending 216 RGB colors to USB device
Packet size: 654 bytes, header: FF FF AA 00 D8 ...
Successfully sent 216 LED colors (654 bytes)
Sent 216 colors to USB device
```

## Troubleshooting

### "Failed to open device /dev/ttyUSB0"

**Solution:**
```bash
# Check if device exists
ls -l /dev/ttyUSB*

# Check permissions
ls -l /dev/ttyUSB0

# Fix permissions
sudo chmod 666 /dev/ttyUSB0
# OR
sudo usermod -a -G dialout $USER && sudo reboot
```

### "Permission denied"

**Solution:**
```bash
sudo usermod -a -G dialout $USER
sudo reboot
```

### Wrong Device Path

**Solution:**
```bash
# Find correct device
ls -l /dev/tty* | grep USB

# Update config.hyperhdr.json
# Change "device": "/dev/ttyUSB0" to your device
```

### Arduino Not Receiving Data

**Check:**
1. Arduino Serial Monitor open? Close it (only one program can use serial port)
2. Baud rate matches? Both config and Arduino sketch must use 460800
3. LED_PIN correct? Check Arduino sketch configuration
4. Power supply adequate? 60mA per LED at full brightness

### Low Frame Rate

**Solutions:**
1. Increase baudrate in config (if hardware supports):
   ```json
   "baudrate": 921600
   ```
   Update Arduino sketch to match!

2. Reduce target FPS:
   ```json
   "target_fps": 30
   ```

3. Check CPU usage:
   ```bash
   top
   ```

## Advanced Usage

### Single Frame Test

```bash
./build/bin/app \
    --config config.hyperhdr.json \
    --live \
    --single-frame \
    --save-debug \
    --verbose
```

### Debug Mode (Static Image)

```bash
./build/bin/app \
    --config config.hyperhdr.json \
    --debug \
    --image img2.png \
    --single-frame \
    --save-debug \
    --verbose
```

### Dual Output (HyperHDR + USB)

Edit `config.hyperhdr.json`:
```json
{
  "hyperhdr": {
    "enabled": true
  },
  "usb": {
    "enabled": true
  }
}
```

Then run normally - data will be sent to both!

## Performance Monitoring

### Check Frame Rate

The application logs FPS every 100 frames:
```
Processed 100 frames, 58.5 FPS
```

### Monitor Serial Communication

Open Arduino Serial Monitor (460800 baud) to see:
```
=== TV LED USB Receiver ===
Max LEDs: 300
Baud rate: 460800
Waiting for data...
Packets received: 150 | LEDs: 216
```

### System Resources

```bash
# CPU usage
top -p $(pgrep -f "bin/app")

# USB device activity
watch -n 1 'ls -lh /dev/ttyUSB0'
```

## Configuration Reference

**Current USB Settings:**
```json
{
  "usb": {
    "enabled": true,
    "device": "/dev/ttyUSB0",
    "baudrate": 460800
  }
}
```

**Baudrate Recommendations:**

| LEDs | Recommended Baudrate | Max FPS @ 60 FPS Target |
|------|----------------------|------------------------|
| 50-100 | 230400 | ~35 FPS |
| 100-200 | 460800 | ~70 FPS ✓ (your setup) |
| 200-300 | 921600 | ~140 FPS |
| 300+ | 1000000+ | 200+ FPS |

Your current setup (216 LEDs @ 460800 baud) is perfectly optimized! 🎉

