# Boat Display — Waveshare ESP32-S3-Touch-LCD-4B + SignalK

A dark-themed, 4-page marine instrument display built on:
- **Hardware:** Waveshare ESP32-S3-Touch-LCD-4B (480×480, ST7701 + GT911)
- **GUI:** LVGL 8.4 with 4 swipeable tab pages
- **Data:** SignalK WebSocket delta stream (WiFi)
- **Framework:** PlatformIO / Arduino

---

## Pages

| Tab | Instruments |
|-----|-------------|
| **NAV** | SOG, STW, Heading (compass arc), COG, GPS Lat/Lon |
| **WIND** | AWS, TWS, TWD, Wind angle rose (AWA + TWA) |
| **DEPTH** | Depth (coloured teal), Water temp, Air temp |
| **ENG** | RPM, Coolant temp, Oil pressure, House volts, Start volts |

---

## Quick Start

### 1. Edit `include/config.h`
```cpp
#define WIFI_SSID        "YourBoatSSID"
#define WIFI_PASSWORD    "YourPassword"
#define SIGNALK_HOST     "192.168.1.100"   // IP or hostname of SignalK server
#define SIGNALK_PORT     3000
```

### 2. Check your board's pin numbers
The Waveshare 4B RGB pinout in `config.h` matches the official wiki.  
If you have a slightly different revision, compare against:
https://www.waveshare.com/wiki/ESP32-S3-Touch-LCD-4B

### 3. Build & Upload (PlatformIO)
```bash
pio run --target upload
pio device monitor
```

---

## Warning Thresholds (colour coding)

These are set in `src/ui.cpp` in the `ui_update()` function:

| Field | Warn colour | Condition |
|-------|-------------|-----------|
| Depth | Yellow | < 2.0 m |
| Engine RPM | Red | > 3500 |
| Coolant temp | Red | > 95 °C |
| Oil pressure | Yellow | < 150 kPa |
| Battery voltage | Yellow | < 12.0 V or > 14.8 V |

Edit the `warn_lo` / `warn_hi` arguments to `set_val_f()` to suit your engine.

---

## Adding More SignalK Paths

Open `src/signalk.cpp` and add to the `parseDelta()` function:
```cpp
else if (strcmp(path, "your.signalk.path") == 0)
    boatSet(gBoat.your_field, v);
```

Then add the field to `include/boat_data.h` and display it in `src/ui.cpp`.

---

## PSRAM Note

The Waveshare 4B has 8 MB OPI PSRAM. The LVGL double-buffer (480 × 120 × 2 bytes × 2 = ~225 KB) is allocated there. If PSRAM init fails, the code falls back to internal DMA RAM with a smaller buffer — this may reduce performance.

---

## Library Versions (pinned in platformio.ini)

| Library | Version |
|---------|---------|
| Arduino_GFX_Library | 1.4.7 |
| lvgl | 8.4.0 |
| TAMC_GT911 | latest from GitHub |
| ArduinoJson | 7.x |
| WebSockets | 2.4.1 |
