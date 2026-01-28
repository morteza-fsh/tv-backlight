#!/bin/bash

# Simple TV monitor - start/stop LED controller based on TV state

TV_IP="176.16.1.111"
CHECK_INTERVAL=5
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
LED_SCRIPT="$SCRIPT_DIR/run_usb_direct.sh"
LED_PID=""

echo "Monitoring TV at $TV_IP (checking every ${CHECK_INTERVAL}s)"
echo "Press Ctrl+C to stop"
echo ""

# Kill LED script on exit
trap 'if [ -n "$LED_PID" ]; then kill $LED_PID 2>/dev/null; wait $LED_PID 2>/dev/null; fi' EXIT INT TERM

# Main loop
TV_WAS_ON=false

while true; do
    # Check if TV is awake
    TV_IS_ON=$(adb -s "$TV_IP:5555" shell dumpsys power 2>/dev/null | grep -i "Wakefulness.*Awake" && echo "yes" || echo "no")
    
    if [ "$TV_IS_ON" = "yes" ]; then
        if [ "$TV_WAS_ON" = false ]; then
            echo "TV ON - Starting LEDs"
            "$LED_SCRIPT" > /tmp/tv_led.log 2>&1 &
            LED_PID=$!
            TV_WAS_ON=true
        fi
    else
        if [ "$TV_WAS_ON" = true ]; then
            echo "TV OFF - Stopping LEDs"
            kill $LED_PID 2>/dev/null
            wait $LED_PID 2>/dev/null
            LED_PID=""
            TV_WAS_ON=false
        fi
    fi
    
    sleep $CHECK_INTERVAL
done
