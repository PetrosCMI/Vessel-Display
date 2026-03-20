#pragma once
#include <stdint.h>
#include <math.h>

enum class SpeedUnit  : uint8_t { KNOTS = 0, KMH, MPH };
enum class DepthUnit  : uint8_t { METERS = 0, FEET };
enum class TempUnit   : uint8_t { CELSIUS = 0, FAHRENHEIT };
enum class PressUnit  : uint8_t { KPA = 0, PSI };

enum AlarmID : uint8_t {
    ALARM_DEPTH_MIN = 0,
    ALARM_DEPTH_MAX,
    ALARM_WIND_SPEED_MAX,
    ALARM_COUNT
};

struct AlarmDef {
    AlarmID     id;
    const char* label;
    float       default_lo;   // SI units (m, m/s). NAN = no lower limit.
    float       default_hi;
};

extern const AlarmDef ALARM_TABLE[ALARM_COUNT];

struct Settings {
    SpeedUnit  speed_unit  = SpeedUnit::KNOTS;
    DepthUnit  depth_unit  = DepthUnit::METERS;
    TempUnit   temp_unit   = TempUnit::CELSIUS;
    PressUnit  press_unit  = PressUnit::KPA;

    bool  alarm_enabled[ALARM_COUNT] = { true, false, true };
    float alarm_lo[ALARM_COUNT]      = { 3.0f, NAN,   NAN  };
    float alarm_hi[ALARM_COUNT]      = { NAN,  20.0f, 15.4f }; // 15.4 m/s ≈ 30 kts

    uint8_t alarm_volume = 70;   // 0–100 (esp_codec_dev percent)
    bool    audio_enabled = true;
    int     brightness_pct = 80; // 0–100
};

extern Settings gSettings;

void settings_init();
void settings_save();
