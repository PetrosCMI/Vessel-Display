#pragma once
#include <stdio.h>
#include <math.h>
#include "settings.h"

// ─────────────────────────────────────────────────────────────
//  All display formatting lives here.
//  Input is always SI (m/s, metres, °C, kPa).
//  Returns const char* pointing to the unit label string.
// ─────────────────────────────────────────────────────────────

inline const char* fmt_speed(float ms, char* buf, size_t len) {
    if (isnan(ms)) { snprintf(buf, len, "---"); return "kts"; }
    switch (gSettings.speed_unit) {
        case SpeedUnit::KMH:  snprintf(buf, len, "%.1f", ms * 3.6f);      return "km/h";
        case SpeedUnit::MPH:  snprintf(buf, len, "%.1f", ms * 2.23694f);  return "mph";
        default:              snprintf(buf, len, "%.1f", ms * 1.94384f);  return "kts";
    }
}

inline const char* fmt_depth(float m, char* buf, size_t len) {
    if (isnan(m)) { snprintf(buf, len, "---"); return "m"; }
    switch (gSettings.depth_unit) {
        case DepthUnit::FEET: snprintf(buf, len, "%.1f", m * 3.28084f); return "ft";
        default:              snprintf(buf, len, "%.1f", m);            return "m";
    }
}

inline const char* fmt_temp(float c, char* buf, size_t len) {
    if (isnan(c)) { snprintf(buf, len, "---"); return "\xc2\xb0""C"; }
    switch (gSettings.temp_unit) {
        case TempUnit::FAHRENHEIT: snprintf(buf, len, "%.1f", c * 9.0f / 5.0f + 32.0f); return "\xc2\xb0""F";
        default:                   snprintf(buf, len, "%.1f", c);                        return "\xc2\xb0""C";
    }
}

inline const char* fmt_pressure(float kpa, char* buf, size_t len) {
    if (isnan(kpa)) { snprintf(buf, len, "---"); return "kPa"; }
    switch (gSettings.press_unit) {
        case PressUnit::PSI: snprintf(buf, len, "%.1f", kpa * 0.14504f); return "psi";
        default:             snprintf(buf, len, "%.1f", kpa);            return "kPa";
    }
}

inline void fmt_angle(float deg, char* buf, size_t len) {
    if (isnan(deg)) snprintf(buf, len, "---\xc2\xb0");
    else            snprintf(buf, len, "%.0f\xc2\xb0", deg);
}

inline void fmt_angle_signed(float deg, char* buf, size_t len) {
    if (isnan(deg)) snprintf(buf, len, "---\xc2\xb0");
    else            snprintf(buf, len, "%+.0f\xc2\xb0", deg);
}

inline void fmt_volts(float v, char* buf, size_t len) {
    if (isnan(v)) snprintf(buf, len, "---");
    else          snprintf(buf, len, "%.2f", v);
}

inline void fmt_rpm(float rpm, char* buf, size_t len) {
    if (isnan(rpm)) snprintf(buf, len, "---");
    else            snprintf(buf, len, "%.0f", rpm);
}

// Convert SI alarm threshold to current display unit for spinbox display
inline float alarm_threshold_display(AlarmID id, float si) {
    if (isnan(si)) return si;
    switch (id) {
        case ALARM_DEPTH_MIN: case ALARM_DEPTH_MAX:
            return (gSettings.depth_unit == DepthUnit::FEET) ? si * 3.28084f : si;
        case ALARM_WIND_SPEED_MAX:
            switch (gSettings.speed_unit) {
                case SpeedUnit::KMH: return si * 3.6f;
                case SpeedUnit::MPH: return si * 2.23694f;
                default:             return si * 1.94384f;
            }
        default: return si;
    }
}

// Convert display unit back to SI for storage
inline float alarm_threshold_to_si(AlarmID id, float disp) {
    if (isnan(disp)) return disp;
    switch (id) {
        case ALARM_DEPTH_MIN: case ALARM_DEPTH_MAX:
            return (gSettings.depth_unit == DepthUnit::FEET) ? disp / 3.28084f : disp;
        case ALARM_WIND_SPEED_MAX:
            switch (gSettings.speed_unit) {
                case SpeedUnit::KMH: return disp / 3.6f;
                case SpeedUnit::MPH: return disp / 2.23694f;
                default:             return disp / 1.94384f;
            }
        default: return disp;
    }
}

inline const char* alarm_threshold_unit(AlarmID id) {
    switch (id) {
        case ALARM_DEPTH_MIN: case ALARM_DEPTH_MAX:
            return (gSettings.depth_unit == DepthUnit::FEET) ? "ft" : "m";
        case ALARM_WIND_SPEED_MAX:
            switch (gSettings.speed_unit) {
                case SpeedUnit::KMH: return "km/h";
                case SpeedUnit::MPH: return "mph";
                default:             return "kts";
            }
        default: return "";
    }
}
