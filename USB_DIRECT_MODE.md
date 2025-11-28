# USB Direct Mode - LED Control via Serial Port

This document explains how to use the USB direct mode to send RGB LED data directly to a USB-connected microcontroller (Arduino, ESP32, etc.) instead of using HyperHDR.

## Overview

The USB direct mode provides a simple, efficient way to control LED strips directly from the C++ TV LED controller without needing HyperHDR as an intermediary. This is useful for:

- **Lower latency**: Direct communication without network overhead
- **Simpler setup**: No need to configure HyperHDR
- **Portable**: Works on any system with USB serial support
- **Custom hardware**: Use any microcontroller that supports serial communication

## Communication Protocol

The protocol uses a simple binary format over USB serial:

### Packet Structure

```
[Header: 3 bytes] [LED Count: 2 bytes] [RGB Data: N×3 bytes] [Checksum: 1 byte]
```

| Field | Size | Description |
|-------|------|-------------|
| Header | 3 bytes | `0xFF 0xFF 0xAA` - Fixed packet header |
| LED Count | 2 bytes | Number of LEDs (big-endian, 0-65535) |
| RGB Data | N×3 bytes | RGB values for each LED (R, G, B) |
| Checksum | 1 byte | XOR of all RGB data bytes |

### Example Packet (3 LEDs)

```
FF FF AA 00 03 FF 00 00 00 FF 00 00 00 FF 00
│  │  │  │  │  └─────┬─────┘ └──┬──┘ └──┬──┘ │
│  │  │  │  │      Red      Green    Blue    Checksum
│  │  │  │  LED Count (3)
│  │  Header
```

## Hardware Setup

### Required Components

1. **Microcontroller**: Arduino Uno/Mega, ESP32, Teensy, etc.
2. **LED Strip**: WS2812B (NeoPixel), APA102, SK6812, etc.
3. **Power Supply**: 5V power supply rated for your LED count
   - Rule of thumb: 60mA per LED at full white brightness
   - Example: 100 LEDs = 6A @ 5V
4. **USB Cable**: USB connection between Raspberry Pi and microcontroller

### Wiring

```
Raspberry Pi ─USB─ Arduino/ESP32 ─Data Pin─ LED Strip
                                            
Power Supply 5V ─────────────────────────── LED Strip VCC
Power Supply GND ───┬──────────────────────  LED Strip GND
                    └──────────────────────  Arduino GND
```

**Important**: Connect the power supply ground to both the LED strip and Arduino ground!

## Software Setup

### 1. Install Arduino/ESP32 Firmware

Upload the example receiver sketch to your microcontroller:

```bash
# Located in the examples directory
examples/arduino_usb_receiver.ino
```

**Configuration in the sketch:**
- `LED_PIN`: GPIO pin connected to LED strip data line (default: 6)
- `MAX_LEDS`: Maximum number of LEDs supported (default: 300)
- `SERIAL_BAUD`: Baud rate (must match config, default: 115200)

**Required Library:**
- FastLED: `Tools` → `Manage Libraries` → Search "FastLED" → Install

### 2. Find USB Device Path

After uploading the sketch, find the USB device:

```bash
# On Linux/Raspberry Pi
ls -l /dev/ttyUSB*
ls -l /dev/ttyACM*

# On macOS
ls -l /dev/tty.usb*
ls -l /dev/cu.usb*
```

Common device paths:
- Arduino Uno: `/dev/ttyACM0` (Linux) or `/dev/tty.usbmodem*` (macOS)
- Arduino Mega: `/dev/ttyACM0` (Linux) or `/dev/tty.usbserial*` (macOS)
- ESP32: `/dev/ttyUSB0` (Linux) or `/dev/cu.usbserial*` (macOS)

### 3. Configure C++ Application

Edit your config file (e.g., `config.hyperhdr.json`):

```json
{
  "hyperhdr": {
    "enabled": false
  },
  
  "usb": {
    "enabled": true,
    "device": "/dev/ttyUSB0",
    "baudrate": 115200
  },
  
  "led_layout": {
    "format": "hyperhdr",
    "hyperhdr": {
      "top": 69,
      "bottom": 69,
      "left": 39,
      "right": 39
    }
  }
}
```

**USB Configuration Options:**
- `enabled`: Set to `true` to enable USB direct mode
- `device`: Serial device path (see step 2)
- `baudrate`: Serial baud rate
  - Common values: 115200 (default), 230400, 460800, 921600
  - Higher baud rates support more LEDs/higher frame rates
  - Must match the Arduino sketch setting

### 4. Set USB Permissions (Linux Only)

Add your user to the `dialout` group:

```bash
sudo usermod -a -G dialout $USER
sudo reboot  # Required for group changes to take effect
```

Or set permissions directly (temporary, lost on reboot):

```bash
sudo chmod 666 /dev/ttyUSB0
```

### 5. Run the Application

```bash
cd build
./bin/app --config config.hyperhdr.json --live
```

You should see:
```
USB controller ready at /dev/ttyUSB0 @ 115200 baud
Sent 216 colors to USB device
```

## Troubleshooting

### "Failed to open device /dev/ttyUSB0"

**Cause**: Permission denied or device doesn't exist

**Solutions**:
1. Check device path: `ls -l /dev/tty*`
2. Check permissions: `ls -l /dev/ttyUSB0`
3. Add user to dialout group (see step 4 above)
4. Try with sudo: `sudo ./bin/app ...` (temporary test only)

### "Checksum error!" on Arduino Serial Monitor

**Cause**: Data corruption during transmission

**Solutions**:
1. Use a shorter USB cable (< 3 meters)
2. Lower the baud rate in both config and Arduino sketch
3. Check USB cable quality
4. Try a different USB port

### LEDs show wrong colors or flicker

**Cause**: Power supply issues or incorrect wiring

**Solutions**:
1. Ensure power supply is adequate (60mA per LED)
2. Connect power supply ground to Arduino ground
3. Add a 470Ω resistor between Arduino data pin and LED strip data input
4. Add a 1000µF capacitor between LED strip VCC and GND

### "Packet timeout" messages

**Cause**: Data not arriving fast enough

**Solutions**:
1. Increase `PACKET_TIMEOUT` in Arduino sketch
2. Reduce the frame rate in config: `"target_fps": 30`
3. Lower baud rate if using long cables

### LEDs freeze or stop updating

**Cause**: Serial buffer overflow or communication stopped

**Solutions**:
1. Press reset button on Arduino
2. Restart the C++ application
3. Increase baud rate for better throughput
4. Reduce number of LEDs if too many for the baud rate

## Performance Considerations

### Maximum Frame Rate vs. LED Count

Frame rate depends on baud rate and LED count:

```
FPS = Baud Rate / (Packet Size × 10 bits/byte)
Packet Size = 6 + (LED_count × 3) bytes
```

Examples:

| LEDs | Baud Rate | Packet Size | Max FPS |
|------|-----------|-------------|---------|
| 100  | 115200    | 306 bytes   | ~38 FPS |
| 100  | 230400    | 306 bytes   | ~75 FPS |
| 200  | 115200    | 606 bytes   | ~19 FPS |
| 200  | 460800    | 606 bytes   | ~76 FPS |
| 300  | 921600    | 906 bytes   | ~102 FPS|

**Recommendation**: Use higher baud rates for more LEDs or higher frame rates.

### Baud Rate Selection

Supported baud rates (must be supported by both USB-serial chip and OS):
- 115200 (default, universally supported)
- 230400 (widely supported)
- 460800 (supported on most devices)
- 921600 (ESP32, FTDI, some Arduino)
- 1000000+ (ESP32, high-quality FTDI chips)

**Test compatibility**:
```bash
# Linux
stty -F /dev/ttyUSB0 921600
echo "test" > /dev/ttyUSB0
```

## Dual Output Mode

You can enable both HyperHDR and USB simultaneously:

```json
{
  "hyperhdr": {
    "enabled": true,
    "host": "127.0.0.1",
    "port": 19400
  },
  
  "usb": {
    "enabled": true,
    "device": "/dev/ttyUSB0",
    "baudrate": 115200
  }
}
```

This sends the same RGB data to both outputs, useful for:
- Previewing on HyperHDR while testing USB hardware
- Controlling multiple LED installations simultaneously
- Debugging and development

## Advanced: Custom Receiver Implementation

You can implement your own receiver in any language. Example in Python:

```python
import serial

ser = serial.Serial('/dev/ttyUSB0', 115200)

while True:
    # Look for header
    if ser.read(1) == b'\xff' and ser.read(1) == b'\xff' and ser.read(1) == b'\xaa':
        # Read LED count
        led_count = int.from_bytes(ser.read(2), 'big')
        
        # Read RGB data
        rgb_data = ser.read(led_count * 3)
        
        # Read checksum
        checksum = ser.read(1)[0]
        
        # Verify checksum
        calc_checksum = 0
        for byte in rgb_data:
            calc_checksum ^= byte
            
        if calc_checksum == checksum:
            # Process RGB data
            for i in range(led_count):
                r = rgb_data[i*3 + 0]
                g = rgb_data[i*3 + 1]
                b = rgb_data[i*3 + 2]
                # Do something with RGB values
                print(f"LED {i}: R={r} G={g} B={b}")
```

## See Also

- [HyperHDR Documentation](https://docs.hyperhdr.eu/)
- [FastLED Library](https://github.com/FastLED/FastLED)
- [Adafruit NeoPixel Guide](https://learn.adafruit.com/adafruit-neopixel-uberguide)

