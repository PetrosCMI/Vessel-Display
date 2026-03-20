#pragma once
#include <Arduino.h>

// ─────────────────────────────────────────────────────────────
//  UNIT ENUMS
// ─────────────────────────────────────────────────────────────
enum class SpeedUnit  : uint8_t { KNOTS = 0, KMH, MPH };
enum class DepthUnit  : uint8_t { METERS = 0, FEET };
enum class TempUnit   : uint8_t { CELSIUS = 0, FAHRENHEIT };
enum class PressUnit  : uint8_t { KPA = 0, PSI };

// ─────────────────────────────────────────────────────────────
//  ALARM DEFINITIONS
//  Each alarm has an ID used to index into the AlarmDef array.
// ─────────────────────────────────────────────────────────────
enum AlarmID : uint8_t {
    ALARM_DEPTH_MIN = 0,
    ALARM_DEPTH_MAX,
    ALARM_WIND_SPEED_MAX,
    ALARM_COUNT   // keep last
};

struct AlarmDef {
    AlarmID     id;
    const char* label;       // shown in Settings UI
    float       default_lo;  // NAN = no lower alarm
    float       default_hi;  // NAN = no upper alarm
    // Units note: thresholds stored in SI (m, m/s) regardless of display unit
};

// The canonical alarm table — alarm.cpp and ui_settings.cpp both use this
extern const AlarmDef ALARM_TABLE[ALARM_COUNT];

// ─────────────────────────────────────────────────────────────
//  SETTINGS STRUCT  (persisted to NVS)
// ─────────────────────────────────────────────────────────────
struct Settings {
    // Units
    SpeedUnit  speed_unit  = SpeedUnit::KNOTS;
    DepthUnit  depth_unit  = DepthUnit::METERS;
    TempUnit   temp_unit   = TempUnit::CELSIUS;
    PressUnit  press_unit  = PressUnit::KPA;

    // Alarm enable flags
    bool alarm_enabled[ALARM_COUNT] = { true, false, true };

    // Alarm thresholds (SI units — metres, m/s)
    float alarm_lo[ALARM_COUNT] = { 3.0f,  NAN,  NAN  };  // depth min 3 m
    float alarm_hi[ALARM_COUNT] = { NAN,  20.0f, 15.4f };  // depth max 20m, wind 30 kts→15.4 m/s

    // Audio
    uint8_t alarm_volume = 200;  // 0-255
    bool    audio_enabled = true;

    // Display
    uint8_t brightness = 200;
};

// ─────────────────────────────────────────────────────────────
//  PUBLIC API
// ─────────────────────────────────────────────────────────────
extern Settings gSettings;

void settings_init();   // load from NVS (or use defaults if first boot)
void settings_save();   // persist to NVS
