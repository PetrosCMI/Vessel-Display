#!/bin/bash
# Pre-Build Verification Script
# Run this before building to catch common issues

echo "==================================="
echo "MQTT Migration - Build Verification"
echo "==================================="
echo ""

# Check if we're in the right directory
if [ ! -f "platformio.ini" ]; then
    echo "❌ ERROR: platformio.ini not found"
    echo "   Run this script from the project root directory"
    exit 1
fi
echo "✓ Project root directory confirmed"

# Check required files exist
echo ""
echo "Checking new MQTT files..."
if [ ! -f "main/boat_mqtt.cpp" ]; then
    echo "❌ ERROR: main/boat_mqtt.cpp not found"
    exit 1
fi
echo "✓ main/boat_mqtt.cpp exists"

if [ ! -f "main/boat_mqtt.h" ]; then
    echo "❌ ERROR: main/boat_mqtt.h not found"
    exit 1
fi
echo "✓ main/boat_mqtt.h exists"

# Check MQTT dependency in idf_component.yml
echo ""
echo "Checking MQTT dependency..."
if grep -q "espressif/esp-mqtt" main/idf_component.yml; then
    echo "✓ MQTT component dependency found"
else
    echo "❌ ERROR: esp-mqtt component not found in main/idf_component.yml"
    echo "   Add: espressif/esp-mqtt: \"^2.0.0\""
    exit 1
fi

# Check CMakeLists.txt includes boat_mqtt.cpp and mqtt component
echo ""
echo "Checking CMakeLists.txt..."
if grep -q "boat_mqtt.cpp" main/CMakeLists.txt; then
    echo "✓ boat_mqtt.cpp listed in SRCS"
else
    echo "❌ ERROR: boat_mqtt.cpp not found in main/CMakeLists.txt"
    exit 1
fi

if grep -q "esp-mqtt" main/CMakeLists.txt; then
    echo "✓ esp-mqtt component in REQUIRES"
else
    echo "❌ ERROR: esp-mqtt not found in REQUIRES in main/CMakeLists.txt"
    exit 1
fi

# Check main.cpp includes boat_mqtt.h
echo ""
echo "Checking main.cpp includes..."
if grep -q '#include "boat_mqtt.h"' main/main.cpp; then
    echo "✓ main.cpp includes boat_mqtt.h"
else
    echo "❌ ERROR: main.cpp doesn't include boat_mqtt.h"
    exit 1
fi

# Check main.cpp calls mqtt_start()
if grep -q 'mqtt_start()' main/main.cpp; then
    echo "✓ main.cpp calls mqtt_start()"
else
    echo "❌ ERROR: main.cpp doesn't call mqtt_start()"
    exit 1
fi

# Check settings.h has ALARM_DATA_TIMEOUT
echo ""
echo "Checking alarm definitions..."
if grep -q 'ALARM_DATA_TIMEOUT' main/settings.h; then
    echo "✓ ALARM_DATA_TIMEOUT defined"
else
    echo "❌ ERROR: ALARM_DATA_TIMEOUT not found in settings.h"
    exit 1
fi

# Check boat_data.h has mqtt_connected
echo ""
echo "Checking boat data structure..."
if grep -q 'mqtt_connected' main/boat_data.h; then
    echo "✓ mqtt_connected flag present"
else
    echo "❌ ERROR: mqtt_connected not found in boat_data.h"
    exit 1
fi

# Check settings has mqtt_host and mqtt_port
echo ""
echo "Checking MQTT settings..."
if grep -q 'mqtt_host' main/settings.h && grep -q 'mqtt_port' main/settings.h; then
    echo "✓ MQTT broker settings present"
else
    echo "❌ ERROR: mqtt_host/mqtt_port not found in settings.h"
    exit 1
fi

# Check alarm.cpp handles DATA_TIMEOUT
echo ""
echo "Checking alarm implementation..."
if grep -q 'ALARM_DATA_TIMEOUT' main/alarm.cpp; then
    echo "✓ Data timeout alarm implemented"
else
    echo "⚠️  WARNING: ALARM_DATA_TIMEOUT not found in alarm.cpp"
    echo "   Alarm might not trigger properly"
fi

# Check UI uses mqtt_connected
echo ""
echo "Checking UI updates..."
if grep -q 'mqtt_connected' main/ui.cpp; then
    echo "✓ UI uses mqtt_connected status"
else
    echo "⚠️  WARNING: ui.cpp still references signalk_connected"
    echo "   Status bar may show wrong connection state"
fi

# Syntax check - try to count functions in boat_mqtt.cpp
echo ""
echo "Checking MQTT client syntax..."
mqtt_funcs=$(grep -c "^void mqtt_" main/boat_mqtt.cpp || echo "0")
if [ "$mqtt_funcs" -ge 2 ]; then
    echo "✓ MQTT client has expected functions (found $mqtt_funcs)"
else
    echo "⚠️  WARNING: Expected mqtt_start/mqtt_stop functions"
fi

# Check for leftover signalk_start() calls
echo ""
echo "Checking for old SignalK calls..."
signalk_calls=$(grep -c "signalk_start\|signalk_stop" main/main.cpp || echo "0")
if [ "$signalk_calls" -eq 0 ]; then
    echo "✓ No signalk_start/stop calls in main.cpp"
else
    echo "⚠️  WARNING: Found $signalk_calls SignalK calls in main.cpp"
    echo "   These should be replaced with mqtt_start/stop"
fi

echo ""
echo "==================================="
echo "Verification Summary"
echo "==================================="
echo ""
echo "If all checks passed, you can build with:"
echo "  pio run"
echo ""
echo "To flash:"
echo "  pio run -t upload"
echo ""
echo "To monitor:"
echo "  pio device monitor"
echo ""
echo "==================================="
echo "Expected First Boot Behavior:"
echo "==================================="
echo "1. Settings reset (schema v6 → v7)"
echo "2. 'MQTT: Connecting to mqtt://192.168.1.100:1883'"
echo "3. 'MQTT: Connected to broker'"
echo "4. 'MQTT: Subscribed to all topics'"
echo "5. Display shows 'MQTT' in green on status bar"
echo ""
echo "If you see errors, check:"
echo "- MQTT broker is running (sudo systemctl status mosquitto)"
echo "- SignalK MQTT plugin is enabled"
echo "- Broker address matches (default: 192.168.1.100:1883)"
echo ""
