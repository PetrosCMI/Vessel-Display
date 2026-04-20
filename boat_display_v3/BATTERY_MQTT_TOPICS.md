# Battery Monitor ESP32 - MQTT Topic Reference

## Quick Reference
Your battery monitor ESP32s should publish to these topics:

### House Bank (Lead Acid)
```
boat/battery/house/voltage    → "13.2"   (plain text, volts)
boat/battery/house/current    → "-15.3"  (plain text, amps, negative = charging)
boat/battery/house/soc        → "87.5"   (plain text, percent 0-100)
```

### House Bank (LiFePO4)
```
boat/battery/house_li/voltage → "13.4"
boat/battery/house_li/current → "-8.2"
boat/battery/house_li/soc     → "92.0"
```

### Starter Battery
```
boat/battery/starter/voltage  → "12.8"
boat/battery/starter/current  → "0.0"
boat/battery/starter/soc      → "95.0"
```

### Forward/Thruster Battery
```
boat/battery/forward/voltage  → "12.6"
boat/battery/forward/current  → "-2.1"
boat/battery/forward/soc      → "78.0"
```

## Publishing Format
The display expects **plain text values** (not JSON) for battery data:

### ✅ Correct Format
```cpp
// In your battery ESP32 code:
mqtt_client.publish("boat/battery/house/voltage", "13.24");
mqtt_client.publish("boat/battery/house/current", "-15.32");
mqtt_client.publish("boat/battery/house/soc", "87.5");
```

### ❌ Incorrect Format (don't use JSON for battery topics)
```cpp
// Don't do this for battery topics:
mqtt_client.publish("boat/battery/house/voltage", "{\"value\": 13.24}");
```

## Example Arduino/ESP-IDF Code

### Arduino (PubSubClient)
```cpp
#include <WiFi.h>
#include <PubSubClient.h>

WiFiClient espClient;
PubSubClient mqtt(espClient);

void setup() {
  mqtt.setServer("192.168.1.100", 1883);
  mqtt.connect("battery_house");
}

void loop() {
  float voltage = readVoltage();  // Your sensor code
  float current = readCurrent();
  float soc = calculateSOC();
  
  char buf[16];
  
  snprintf(buf, sizeof(buf), "%.2f", voltage);
  mqtt.publish("boat/battery/house/voltage", buf);
  
  snprintf(buf, sizeof(buf), "%.2f", current);
  mqtt.publish("boat/battery/house/current", buf);
  
  snprintf(buf, sizeof(buf), "%.1f", soc);
  mqtt.publish("boat/battery/house/soc", buf);
  
  delay(1000);  // Publish every second
}
```

### ESP-IDF (native)
```cpp
#include "mqtt_client.h"

void publish_battery_data(float voltage, float current, float soc) {
    char buf[16];
    
    snprintf(buf, sizeof(buf), "%.2f", voltage);
    esp_mqtt_client_publish(mqtt_client, "boat/battery/house/voltage", buf, 0, 0, 0);
    
    snprintf(buf, sizeof(buf), "%.2f", current);
    esp_mqtt_client_publish(mqtt_client, "boat/battery/house/current", buf, 0, 0, 0);
    
    snprintf(buf, sizeof(buf), "%.1f", soc);
    esp_mqtt_client_publish(mqtt_client, "boat/battery/house/soc", buf, 0, 0, 0);
}
```

## Testing Your Battery ESP32s

### Verify Publishing
From your Pi or any device on the network:
```bash
# Subscribe to all battery topics
mosquitto_sub -h 192.168.1.100 -t 'boat/battery/#' -v

# You should see:
boat/battery/house/voltage 13.24
boat/battery/house/current -15.32
boat/battery/house/soc 87.5
boat/battery/starter/voltage 12.81
...
```

### Test the Display is Receiving
On the display ESP32 monitor:
```bash
pio device monitor

# Look for:
[D][MQTT] Topic: boat/battery/house/voltage, Data: 13.24
[D][MQTT] Topic: boat/battery/house/current, Data: -15.32
```

## Troubleshooting

### Display shows "---" for battery data
- Check ESP32 is publishing (use mosquitto_sub)
- Check topic names match exactly (case-sensitive!)
- Check MQTT broker is running
- Check WiFi connection on battery ESP32

### Display shows old/stale battery data
- Check publish rate (should be 1-5 seconds)
- Check MQTT client is reconnecting after WiFi drops
- Add retained flag: `mqtt.publish("topic", value, true)` so last value persists

### Current shows wrong sign
Convention: **Negative = Charging**, **Positive = Discharging**
If backwards, fix in your battery ESP32 code:
```cpp
current = -current;  // Flip the sign
```

## Best Practices

### Publish Rate
- **1 second**: Good for monitoring, responsive display
- **5 seconds**: Lower network traffic, still responsive
- **10 seconds**: Light network use, adequate for slow changes

### QoS Level
- **QoS 0**: Fire and forget (recommended for high-frequency data)
- **QoS 1**: At least once delivery (use for important state changes)

### Retained Messages
Consider using retained messages so display gets last value immediately on reconnect:
```cpp
mqtt.publish("boat/battery/house/voltage", buf, true);  // retained=true
```

### Client ID
Each battery ESP32 needs a unique client ID:
```cpp
mqtt.connect("battery_house");    // House bank ESP32
mqtt.connect("battery_starter");  // Starter battery ESP32
mqtt.connect("battery_forward");  // Forward thruster ESP32
```

## Topic Naming Convention

The display expects this exact structure:
```
boat/battery/{bank_name}/{parameter}

Where:
  {bank_name} = house | house_li | starter | forward
  {parameter} = voltage | current | soc
```

If you need to change topic names, edit `main/boat_mqtt.h` on the display ESP32.
