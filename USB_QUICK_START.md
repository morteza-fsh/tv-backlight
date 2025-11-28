# USB Direct Mode - Quick Start Guide

Get started with USB direct LED control in 5 minutes!

## Prerequisites

- Arduino Uno/Mega, ESP32, or compatible microcontroller
- WS2812B LED strip
- USB cable
- 5V power supply (rated for your LED count)

## Step 1: Upload Arduino Sketch

1. Open Arduino IDE
2. Install FastLED library: `Tools` → `Manage Libraries` → Search "FastLED" → Install
3. Open `examples/arduino_usb_receiver.ino`
4. Configure settings (if needed):
   ```cpp
   #define LED_PIN        6      // Your LED data pin
   #define MAX_LEDS       300    // Maximum LEDs
   #define SERIAL_BAUD    115200 // Baud rate
   ```
5. Select your board: `Tools` → `Board`
6. Select your port: `Tools` → `Port`
7. Upload: Click Upload button

## Step 2: Wire Hardware

```
Raspberry Pi ─USB─ Arduino ─Pin 6─ LED Strip Data
                                            
Power Supply 5V ─────────────────────────── LED Strip VCC
Power Supply GND ───┬──────────────────────  LED Strip GND
                    └──────────────────────  Arduino GND
```

**Important**: Connect grounds together!

## Step 3: Find USB Device

```bash
# Linux
ls -l /dev/ttyUSB* /dev/ttyACM*

# macOS
ls -l /dev/tty.usb* /dev/cu.usb*
```

Common paths:
- Arduino: `/dev/ttyACM0` (Linux) or `/dev/tty.usbmodem*` (macOS)
- ESP32: `/dev/ttyUSB0` (Linux) or `/dev/cu.usbserial*` (macOS)

## Step 4: Configure Application

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
  }
}
```

## Step 5: Set Permissions (Linux Only)

```bash
sudo usermod -a -G dialout $USER
sudo reboot
```

Or temporarily:
```bash
sudo chmod 666 /dev/ttyUSB0
```

## Step 6: Run!

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

### Permission Denied
```bash
sudo usermod -a -G dialout $USER
sudo reboot
```

### Wrong Device Path
```bash
ls -l /dev/tty*
# Update config.json with correct path
```

### Low Frame Rate
Increase baud rate in both Arduino sketch and config:
- Arduino: `#define SERIAL_BAUD 460800`
- Config: `"baudrate": 460800`

### LEDs Flicker
- Check power supply (60mA per LED)
- Add 470Ω resistor between Arduino pin and LED data
- Connect all grounds together

## Need More Help?

See full documentation: [USB_DIRECT_MODE.md](USB_DIRECT_MODE.md)

## Test Your Setup

Open Arduino Serial Monitor (115200 baud) - you should see:
```
=== TV LED USB Receiver ===
Max LEDs: 300
Baud rate: 115200
Waiting for data...
Packets received: 150 | LEDs: 216
```

When the C++ app runs, you'll see packet counts increasing.

Enjoy your direct USB LED control! 🎉

