#!/usr/bin/env python3
"""
Turn off all LEDs by sending Adalight protocol command
"""
import sys
import time
import serial
import json

def turn_off_leds(device, baudrate, led_count):
    """Send Adalight command to turn off all LEDs"""
    try:
        # Open serial port with no DTR/RTS reset
        ser = serial.Serial(
            port=device,
            baudrate=baudrate,
            timeout=2,
            # Prevent Arduino reset on connection
            dsrdtr=False,
            rtscts=False
        )
        
        # Give the port a moment to stabilize
        time.sleep(0.1)
        
        # Flush any existing data
        ser.reset_input_buffer()
        ser.reset_output_buffer()
        
        # Build Adalight packet: "Ada" + hi + lo + checksum + (R,G,B)*led_count
        hi = (led_count - 1) >> 8
        lo = (led_count - 1) & 0xFF
        chk = (hi ^ lo ^ 0x55) & 0xFF
        
        # Header
        packet = bytearray(b'Ada')
        packet.append(hi)
        packet.append(lo)
        packet.append(chk)
        
        # All LEDs to 0 (black)
        packet.extend([0, 0, 0] * led_count)
        
        # Send multiple times to ensure receipt
        for _ in range(3):
            ser.write(packet)
            ser.flush()
            time.sleep(0.05)
        
        ser.close()
        print("✓ LEDs turned off")
        return True
        
    except serial.SerialException as e:
        print(f"⚠ Serial error: {e}")
        return False
    except Exception as e:
        print(f"⚠ Error: {e}")
        return False

def main():
    if len(sys.argv) < 4:
        print("Usage: turn_off_leds.py <device> <baudrate> <led_count>")
        sys.exit(1)
    
    device = sys.argv[1]
    baudrate = int(sys.argv[2])
    led_count = int(sys.argv[3])
    
    success = turn_off_leds(device, baudrate, led_count)
    sys.exit(0 if success else 1)

if __name__ == "__main__":
    main()
