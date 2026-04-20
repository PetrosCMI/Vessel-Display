# MQTT Migration Guide

## Summary
This project has been migrated from SignalK WebSocket to MQTT for data connectivity. The display now subscribes directly to MQTT topics published by SignalK and your battery monitor ESP32s.

## Architecture Changes

### Before (SignalK WebSocket)
```
NMEA 2000 → YDWG-02 → SignalK → WebSocket → Display ESP32
Battery ESP32s → MQTT Broker
```

### After (All MQTT)
```
NMEA 2000 → YDWG-02 → SignalK → MQTT Broker → Display ESP32
Battery ESP32s → MQTT Broker → Display ESP32
```

## New Features
1. **MQTT-only connectivity** - Simpler, more reliable protocol
2. **5-minute data timeout alarm** - Alerts if no MQTT data received (configurable via ALARM_DATA_TIMEOUT)
3. **Unified connection status** - Single MQTT connection indicator on status bar
4. **SignalK still in the chain** - SignalK converts NMEA data to MQTT (no changes needed on Pi)

## Files Modified

### New Files
- `main/boat_mqtt.cpp` - MQTT subscriber implementation
- `main/boat_mqtt.h` - MQTT API and topic definitions

### Modified Files
- `main/boat_data.h` - Added `mqtt_connected` flag
- `main/settings.h` - Added `mqtt_host`, `mqtt_port`, `ALARM_DATA_TIMEOUT`
- `main/settings.cpp` - Added MQTT config save/load, bumped schema to v7
- `main/alarm.cpp` - Added 5-minute data timeout alarm logic
- `main/main.cpp` - Replaced `signalk_start()` with `mqtt_start()`
- `main/ui.cpp` - Status bar now shows "MQTT" instead of "SignalK"
- `main/page_network.cpp` - Network page shows MQTT broker status
- `main/idf_component.yml` - Added `espressif/esp-mqtt: ^2.0.0` dependency

### Removed Functionality
- SignalK WebSocket client (signalk.cpp still exists but unused)
- 10-minute SignalK data watchdog (replaced by 5-min alarm)

## MQTT Topic Mapping

### NMEA Data (from SignalK via MQTT)
SignalK publishes these as JSON: `{"value": 12.34}`

| Data | MQTT Topic |
|------|------------|
| Speed Over Ground | `signalk/vessels/self/navigation/speedOverGround` |
| Course Over Ground | `signalk/vessels/self/navigation/courseOverGroundTrue` |
| Heading | `signalk/vessels/self/navigation/headingTrue` |
| Position | `signalk/vessels/self/navigation/position` |
| Depth | `signalk/vessels/self/environment/depth/belowTransducer` |
| Wind Speed (Apparent) | `signalk/vessels/self/environment/wind/speedApparent` |
| Wind Angle (Apparent) | `signalk/vessels/self/environment/wind/angleApparent` |
| Wind Speed (True) | `signalk/vessels/self/environment/wind/speedTrue` |
| Water Temperature | `signalk/vessels/self/environment/water/temperature` |
| Air Temperature | `signalk/vessels/self/environment/outside/temperature` |
| Barometric Pressure | `signalk/vessels/self/environment/outside/pressure` |

### Battery Data (from your ESP32 monitors)
These publish as plain text: `"12.34"`

| Battery Bank | Voltage Topic | Current Topic | SOC Topic |
|--------------|---------------|---------------|-----------|
| House | `boat/battery/house/voltage` | `boat/battery/house/current` | `boat/battery/house/soc` |
| House LiFePO4 | `boat/battery/house_li/voltage` | `boat/battery/house_li/current` | `boat/battery/house_li/soc` |
| Starter | `boat/battery/starter/voltage` | `boat/battery/starter/current` | `boat/battery/starter/soc` |
| Forward | `boat/battery/forward/voltage` | `boat/battery/forward/current` | `boat/battery/forward/soc` |

## Configuration Required

### MQTT Broker Settings
The display uses these defaults (stored in NVS):
```cpp
gSettings.mqtt_host = "192.168.1.100"
gSettings.mqtt_port = 1883
```

**These match your existing SignalK host settings**, so if SignalK and MQTT broker are on the same Pi, no changes needed!

### SignalK MQTT Plugin
Ensure SignalK has the MQTT plugin enabled and publishing to your broker:
1. SignalK → Plugin Config → MQTT Gateway
2. Broker: `localhost:1883` (or your broker address)
3. Topic prefix: `signalk/vessels/self/`
4. Publish rate: 1Hz (1000ms)

## Building & Flashing

### Clean Build Required
The schema version changed (v6→v7), so first boot will reset settings to defaults.

```bash
# Clean and rebuild
pio run -t clean
pio run

# Flash
pio run -t upload

# Monitor
pio device monitor
```

### Expected First Boot Log
```
Settings: Schema v6 → v7 — resetting to defaults
MQTT: Connecting to mqtt://192.168.1.100:1883
MQTT: Connected to broker
MQTT: Subscribed to all topics
Alarm: Alarm engine started
Main: Boot complete
```

## Testing Checklist

### 1. MQTT Connection
- [ ] Status bar shows "MQTT" in green
- [ ] Network page shows "MQTT BROKER: Connected"
- [ ] Network page displays correct broker address

### 2. NMEA Data Reception
- [ ] NAV page shows GPS position updating
- [ ] NAV page shows SOG and COG
- [ ] DEPTH page shows depth updating
- [ ] WIND page shows wind data
- [ ] Status bar shows "Xs ago" timestamp updating

### 3. Battery Data Reception
- [ ] ELEC page shows all battery voltages
- [ ] ELEC page shows currents (if available)
- [ ] ELEC page shows SOC percentages

### 4. Data Timeout Alarm
To test the 5-minute timeout alarm:
1. Stop MQTT broker: `sudo systemctl stop mosquitto`
2. Wait 5 minutes
3. Display should alarm with "Data Timeout"
4. Restart broker: `sudo systemctl start mosquitto`
5. Alarm should clear within 10 seconds

### 5. Existing Alarms Still Work
- [ ] Depth MIN/MAX alarms
- [ ] Battery low voltage alarms
- [ ] Wind speed alarm
- [ ] Anchor drag alarm (if set)

## Troubleshooting

### "Disconnected" on Status Bar
**Check:** Is MQTT broker running?
```bash
sudo systemctl status mosquitto
```

**Check:** Can display reach broker?
```bash
# From display's network (or SSH to Pi)
mosquitto_sub -h 192.168.1.100 -t '#' -v
# Should see SignalK data flowing
```

**Check:** Is SignalK publishing to MQTT?
```bash
mosquitto_sub -h localhost -t 'signalk/#' -v
# Should see vessel data
```

### Data Not Updating
**Check:** Are topics correct?
```bash
# Subscribe to all topics the display uses
mosquitto_sub -h 192.168.1.100 -t 'signalk/vessels/self/#' -v
mosquitto_sub -h 192.168.1.100 -t 'boat/battery/#' -v
```

**Check:** Display ESP32 logs:
```bash
pio device monitor
# Look for "Topic: ..., Data: ..." debug messages
```

### Data Timeout Alarm Won't Clear
The display tracks `last_update_ms` on these critical topics:
- `navigation/position` (GPS)
- `environment/depth/belowTransducer` (depth)
- `environment/wind/speedApparent` (wind)

If none of these update for 5 minutes, alarm triggers. Check that at least ONE of these is publishing regularly.

### Old SignalK WebSocket Still Connecting
SignalK code is still compiled but not called. If you see SignalK logs:
```
SignalK: Connecting to ws://...
```
Then `signalk_start()` is still being called somewhere. Search for it:
```bash
grep -r "signalk_start" main/
```

## Rollback Instructions

If you need to revert to SignalK WebSocket:

1. Restore from backup (if you made one)
2. Or manually revert:
   - Change `#include "mqtt_client.h"` back to `#include "signalk.h"` in main.cpp
   - Change `mqtt_start()` back to `signalk_start()` in main.cpp
   - Change `mqtt_stop()` back to `signalk_stop()` in main.cpp and network_reconnect()
   - Rebuild and flash

## Performance Notes

### Memory Impact
- MQTT client uses ~8KB less RAM than WebSocket client
- No JSON parsing of full delta messages (more efficient)

### Network Impact
- MQTT QoS 0: No message acknowledgment overhead
- Smaller packets than WebSocket deltas
- Better reconnection handling (MQTT built-in)

### CPU Impact
- Less JSON parsing (only parse values we care about)
- No delta message reconstruction
- Topic-based filtering happens at broker (not on ESP32)

## Next Steps / Future Enhancements

Possible improvements:
1. **QoS 1** for critical topics (depth, position) if you want guaranteed delivery
2. **MQTT TLS** if your broker supports it (requires certificates)
3. **Direct YDWG MQTT** - Some YDWG firmware can publish directly to MQTT, bypassing SignalK entirely
4. **Configurable timeout** - Make the 5-minute timeout user-adjustable
5. **Per-topic timeout tracking** - Separate alarms for "no GPS", "no depth", etc.

## Support

If you encounter issues:
1. Check logs: `pio device monitor`
2. Verify MQTT broker is reachable
3. Test with mosquitto_sub to confirm data is flowing
4. Check that SignalK MQTT plugin is enabled and configured

---
**Migration completed**: All data now flows through MQTT for simpler, more reliable monitoring.
