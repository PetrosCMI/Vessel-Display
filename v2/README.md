# Boat Display v2
### Waveshare ESP32-S3-Touch-LCD-4B + SignalK + I2S Audio

A dark-themed, fully configurable marine instrument display.

---

## Features

- **5 swipeable pages:** NAV, WIND, DEPTH, ENGINE, SETTINGS
- **Live SignalK data** via WebSocket delta stream over WiFi
- **Configurable units** (speed, depth, temperature, pressure) — set on screen, saved to flash
- **Audio alarms** via I2S DAC (MAX98357 or similar):
  - Shallow water (critical — continuous pattern)
  - Max depth exceeded
  - Max wind speed exceeded
  - All thresholds configurable on screen with +/− spinboxes
- **NVS persistence** — all settings survive power cycles
- **Flashing bell icon** in status bar when any alarm is active
- **Silence button** on Settings page
- **Colour-coded values** — yellow = caution, red = alert
- **Self-registering page pattern** — adding a new page = one new .cpp file

---

## Hardware

| Component | Detail |
|-----------|--------|
| MCU/Display | Waveshare ESP32-S3-Touch-LCD-4B (480×480, ST7701, GT911) |
| Audio | MAX98357 I2S DAC/amp (or any I2S DAC) |

### I2S Wiring (MAX98357)

| MAX98357 | ESP32-S3 GPIO |
|----------|--------------|
| BCLK     | GPIO 38      |
| LRC      | GPIO 39      |
| DIN      | GPIO 37      |
| VIN      | 3.3V or 5V   |
| GND      | GND          |
| SD       | 3.3V (always on) |

Change pins in `include/config.h` if needed.

---

## Quick Start

1. Edit `include/config.h` — WiFi credentials, SignalK IP, I2S pins
2. `pio run --target upload`
3. Configure units and alarm thresholds on the **SETTINGS** tab

---

## Pages

| Tab | Instruments |
|-----|-------------|
| NAV | SOG, STW, Heading compass, COG, GPS Lat/Lon |
| WIND | AWS, TWS, TWD, Wind angle rose (AWA/TWA) |
| DEPTH | Depth (alarm-aware colour), Water temp, Air temp |
| ENGINE | RPM, Coolant, Oil pressure, House V, Start V |
| SETTINGS | Units, Alarm thresholds, Volume, Brightness, Silence |

---

## Alarm Behaviour

| Alarm | Default | Pattern |
|-------|---------|---------|
| Depth MIN (shallow) | 3.0 m | Continuous — critical |
| Depth MAX | 20.0 m | Triple beep |
| Wind speed MAX | ~30 kts | Triple beep |

- Repeats every 30s while condition persists
- Re-arms automatically when condition clears
- Silence button suppresses tones for current event only

---

## Adding a New Page

Create `src/page_mypage.cpp`:

```cpp
#include "page_registry.h"
#include "ui.h"
#include "boat_data.h"
#include "units.h"
#include <lvgl.h>

static InstrCard my_card;

static void build(lv_obj_t* tab) {
    my_card = make_instr_card(tab, "MY VALUE", "unit", true);
    lv_obj_set_size(my_card.card, 454, 180);
    lv_obj_set_pos(my_card.card, 0, 0);
}

static void update() {
    BoatData d = boatDataSnapshot();
    char buf[24];
    fmt_speed(d.sog_kts / 1.94384f, buf, sizeof(buf));
    instr_card_set(my_card, buf, "kts");
}

static struct MyPageReg {
    MyPageReg() { ui_register_page({ LV_SYMBOL_EYE_OPEN " MY", build, update }); }
} _reg;
```

Done. The tab appears automatically.

---

## Adding a New Alarm

1. Add ID to `enum AlarmID` in `include/settings.h`
2. Add row to `ALARM_TABLE` in `src/settings.cpp`
3. Add `evaluate(ALARM_MY_NEW, d.my_field)` in `alarm_tick()` in `src/alarm.cpp`

The Settings page picks it up with no UI changes needed.

---

## File Map

```
include/
  config.h          ← EDIT THIS — WiFi, SignalK, pins
  boat_data.h       ← Shared sensor data + mutex helpers
  settings.h        ← Settings struct, alarm table, unit enums
  units.h           ← All unit conversion + fmt functions
  alarm.h           ← Alarm engine interface
  ui.h              ← Shared styles, colours, InstrCard
  page_registry.h   ← Page self-registration
  signalk.h

src/
  main.cpp          ← Init + main loop
  settings.cpp      ← NVS persistence + alarm table
  signalk.cpp       ← WebSocket + delta parser
  alarm.cpp         ← I2S audio task + alarm evaluation
  ui.cpp            ← LVGL shell: status bar + tabview
  ui_helpers.cpp    ← InstrCard widget
  page_registry.cpp ← Page vector
  page_nav.cpp      ← NAV page
  page_wind.cpp     ← WIND page
  page_depth.cpp    ← DEPTH page
  page_engine.cpp   ← ENGINE page
  page_settings.cpp ← SETTINGS page
  lv_conf.h         ← LVGL config
```
