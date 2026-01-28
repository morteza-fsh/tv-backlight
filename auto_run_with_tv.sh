#!/bin/bash

# Simple TV monitor - start/stop LED controller based on TV state

TV_IP="176.16.1.111"
CHECK_INTERVAL=5
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
LED_SCRIPT="$SCRIPT_DIR/run_usb_direct.sh"
LED_PID=""

# Function to connect to TV via ADB
connect_to_tv() {
    echo "Connecting to TV at $TV_IP:5555..."
    
    # Try to connect
    adb connect "$TV_IP:5555" > /dev/null 2>&1
    sleep 2
    
    # Verify connection by checking devices list
    if adb devices | grep -q "$TV_IP:5555.*device$"; then
        echo "✓ Successfully connected to TV"
        return 0
    else
        echo "✗ Failed to connect to TV"
        return 1
    fi
}

# Ensure TV is connected via ADB
MAX_RETRIES=3
RETRY_COUNT=0
while [ $RETRY_COUNT -lt $MAX_RETRIES ]; do
    if connect_to_tv; then
        break
    fi
    RETRY_COUNT=$((RETRY_COUNT + 1))
    if [ $RETRY_COUNT -lt $MAX_RETRIES ]; then
        echo "Retrying ($RETRY_COUNT/$MAX_RETRIES)..."
        sleep 2
    else
        echo "Failed to connect to TV after $MAX_RETRIES attempts. Exiting."
        exit 1
    fi
done

echo ""

# Detect if we should use -s flag or not
ADB_DEVICE_COUNT=$(adb devices | grep -v "List of devices" | grep -c "device$")
if [ "$ADB_DEVICE_COUNT" -eq 1 ]; then
    ADB_CMD="adb"
    echo "Monitoring TV (single ADB device, checking every ${CHECK_INTERVAL}s)"
else
    ADB_CMD="adb -s $TV_IP:5555"
    echo "Monitoring TV at $TV_IP:5555 (checking every ${CHECK_INTERVAL}s)"
fi
echo "Press Ctrl+C to stop"
echo ""

# Cleanup function
cleanup() {
    echo ""
    echo "Shutting down..."
    if [ -n "$LED_PID" ] && kill -0 "$LED_PID" 2>/dev/null; then
        echo "Stopping LED controller..."
        kill "$LED_PID" 2>/dev/null
        # Wait up to 3 seconds for graceful shutdown
        for i in {1..6}; do
            if ! kill -0 "$LED_PID" 2>/dev/null; then
                break
            fi
            sleep 0.5
        done
        # Force kill if still running
        if kill -0 "$LED_PID" 2>/dev/null; then
            echo "Force stopping LED controller..."
            kill -9 "$LED_PID" 2>/dev/null
        fi
    fi
    exit 0
}

# Set up trap for cleanup
trap cleanup EXIT INT TERM

# Main loop
TV_WAS_ON=false

while true; do
    # Check if TV is awake
    WAKEFULNESS=$($ADB_CMD shell dumpsys power 2>/dev/null | grep -i "Wakefulness")
    
    # If connection lost, try to reconnect
    if [ -z "$WAKEFULNESS" ]; then
        echo "[$(date '+%H:%M:%S')] Connection lost, attempting to reconnect..."
        if connect_to_tv; then
            echo "[$(date '+%H:%M:%S')] Reconnected successfully"
            continue
        else
            echo "[$(date '+%H:%M:%S')] Reconnection failed, will retry next cycle"
        fi
    fi
    
    # Debug output
    echo "[$(date '+%H:%M:%S')] Wakefulness: $WAKEFULNESS"
    
    # Check if Wakefulness contains "Awake"
    if echo "$WAKEFULNESS" | grep -qi "Awake"; then
        TV_IS_ON="yes"
    else
        TV_IS_ON="no"
    fi
    
    echo "[$(date '+%H:%M:%S')] TV status: $TV_IS_ON"
    
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
            if [ -n "$LED_PID" ] && kill -0 "$LED_PID" 2>/dev/null; then
                kill "$LED_PID" 2>/dev/null
                # Don't wait indefinitely
                sleep 1
            fi
            LED_PID=""
            TV_WAS_ON=false
        fi
    fi
    
    sleep $CHECK_INTERVAL
done
