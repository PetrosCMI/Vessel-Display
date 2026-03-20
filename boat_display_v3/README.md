# Boat Display — ESP-IDF + Waveshare BSP

ESP-IDF v5.1+ project using the official Waveshare BSP for the
ESP32-S3-Touch-LCD-4B. No Arduino dependency.

---

## Prerequisites

- **ESP-IDF v5.1 or later** installed and activated (`idf.py --version`)
- VS Code with the Espressif IDF extension (recommended), or CLI
- The board connected via USB-C

---

## Quick Start

### 1. Edit `main/config.h`
```c
#define WIFI_SSID        "YourBoatSSID"
#define WIFI_PASSWORD    "YourPassword"
#define SIGNALK_HOST     "192.168.1.100"
#define SIGNALK_PORT     3000
```

### 2. Build & flash
```bash
idf.py set-target esp32s3
idf.py build
idf.py -p /dev/ttyUSB0 flash monitor
```

On first build, the IDF Component Manager fetches all dependencies
automatically (~2–5 min depending on connection). Subsequent builds
are fast.

---

## Architecture

### Why ESP-IDF instead of Arduino?

The Waveshare BSP (`bsp/esp32_s3_touch_lcd_4b`) is an ESP-IDF component
that encapsulates all hardware init:

| BSP call | What it replaces |
|----------|-----------------|
| `bsp_i2c_init()` | Wire.begin() + manual I2C config |
| `bsp_display_start()` | Arduino_GFX + LVGL buffer setup + display task |
| `bsp_display_get_input_dev()` | TAMC_GT911 touch driver |
| `bsp_audio_init()` + `bsp_audio_codec_speaker_init()` | Raw I2S driver init |
| `bsp_display_brightness_set(pct)` | ledcWrite() |
| `bsp_display_lock()` / `bsp_display_unlock()` | Hand-rolled LVGL mutex |

This also means no more Arduino core version conflicts.

### Task structure

| Task | Core | Priority | Notes |
|------|------|----------|-------|
| LVGL port task | 1 | high | Started by `bsp_display_start()` |
| SignalK WS task | any | medium | Started by `esp_websocket_client` |
| alarm_tick_task | any | 3 | 1 Hz alarm evaluation |
| tone_task | 0 | 5 | Audio output, blocks on queue |
| app_main | 1 | — | Returns after boot |

### LVGL threading

`bsp_display_start()` launches an internal LVGL task. **All `lv_...()` calls
must be wrapped in `bsp_display_lock(0)` / `bsp_display_unlock()`.**

Exception: code called from `lv_timer_create()` callbacks runs inside the
LVGL task and is already locked.

The `ui_update()` function is registered as an LVGL timer callback (250 ms),
so it runs safely without explicit locking. If you call UI functions from
other tasks, always lock first.

### Audio

The board uses an **ES8311 audio codec** (not a bare DAC like MAX98357).
The BSP initialises it via I2C. Audio output goes through
`esp_codec_dev_write(speaker, pcm_samples, byte_count)`.

`alarm.cpp` synthesises sine-wave tones and writes them via this API.
Volume is set with `esp_codec_dev_set_out_vol(speaker, 0–100)`.

---

## Pages

| Tab | Instruments |
|-----|-------------|
| NAV | SOG, STW, Heading compass, COG, GPS Lat/Lon |
| WIND | AWS, TWS, TWD, Wind angle rose (AWA + TWA) |
| DEPTH | Depth (teal, alarm-aware), Water temp, Air temp |
| ENGINE | RPM, Coolant, Oil pressure, House V, Start V |
| SETTINGS | Units, Alarm thresholds, Volume, Brightness, Silence |

---

## Alarms

| Alarm | Default SI threshold | Display |
|-------|---------------------|---------|
| Depth MIN | 3.0 m | Continuous critical pattern |
| Depth MAX | 20.0 m | Triple beep |
| Wind Speed MAX | 15.4 m/s (~30 kts) | Triple beep |

Thresholds are configurable on the SETTINGS page and saved to NVS.

---

## Adding a New Page

Create `main/page_mypage.cpp`:

```cpp
#include "page_registry.h"
#include "ui.h"
#include "boat_data.h"
#include "units.h"
#include "lvgl.h"

static InstrCard my_card;

static void build(lv_obj_t* tab) {
    my_card = make_instr_card(tab, "MY VALUE", "unit", true);
    lv_obj_set_size(my_card.card, 454, 180);
    lv_obj_set_pos(my_card.card, 0, 0);
}

static void update(void) {
    BoatData d = boatDataSnapshot();
    char buf[24];
    fmt_speed(d.sog_ms, buf, sizeof(buf));
    instr_card_set(my_card, buf, "kts");
}

static struct MyReg {
    MyReg() { ui_register_page({ LV_SYMBOL_EYE_OPEN " MY", build, update }); }
} _reg;
```

Then add `"page_mypage.cpp"` to the `SRCS` list in `main/CMakeLists.txt`.

---

## Adding a New Alarm

1. Add ID to `enum AlarmID` in `main/settings.h`
2. Add row to `ALARM_TABLE[]` in `main/settings.cpp`
3. Add `evaluate(ALARM_MY_NEW, d.my_field_si)` in `alarm_tick()` in `main/alarm.cpp`

The Settings page picks it up automatically — no UI code needed.

---

## File Map

```
main/
  CMakeLists.txt        ← source list + IDF component dependencies
  idf_component.yml     ← external component manifest (BSP, LVGL, etc.)
  config.h              ← EDIT THIS: WiFi, SignalK IP
  main.cpp              ← app_main: BSP init, WiFi, UI, tasks
  boat_data.h/cpp       ← Shared sensor state + mutex helpers
  settings.h/cpp        ← Unit/alarm settings + NVS persistence
  units.h               ← All unit conversion + formatting
  signalk.h/cpp         ← WebSocket client (esp_websocket_client)
  alarm.h/cpp           ← ES8311 audio + alarm evaluation
  ui.h/cpp              ← LVGL shell: status bar, tabview
  ui_helpers.cpp        ← InstrCard widget
  page_registry.h/cpp   ← Self-registration pattern
  page_nav.cpp          ← NAV page
  page_wind.cpp         ← WIND page
  page_depth.cpp        ← DEPTH page
  page_engine.cpp       ← ENGINE page
  page_settings.cpp     ← SETTINGS page
sdkconfig.defaults      ← Kconfig defaults (PSRAM, BSP options, WiFi)
CMakeLists.txt          ← Top-level project file
```
