#pragma once
#include <Arduino.h>
#include <math.h>
#include "settings.h"

// ─────────────────────────────────────────────────────────────
//  UNITS  — all formatting goes through these helpers.
//  Pages never do unit arithmetic directly.
//
//  Each fmt_X() function:
//    • Takes the raw SI value (m/s, metres, K, Pa)
//    • Reads gSettings for the chosen unit
//    • Writes a formatted string into `buf` (caller supplies)
//    • Returns a const char* pointing to the unit label string
// ─────────────────────────────────────────────────────────────

// ── Speed  (SI input: m/s) ───────────────────────────────────
inline const char* fmt_speed(float ms, char* buf, size_t len) {
    if (isnan(ms)) { snprintf(buf, len, "---"); return "kts"; }
    switch (gSettings.speed_unit) {
        case SpeedUnit::KMH:
            snprintf(buf, len, "%.1f", ms * 3.6f);
            return "km/h";
        case SpeedUnit::MPH:
            snprintf(buf, len, "%.1f", ms * 2.23694f);
            return "mph";
        default: // KNOTS
            snprintf(buf, len, "%.1f", ms * 1.94384f);
            return "kts";
    }
}

// Convenience: input already in knots
inline const char* fmt_speed_kts(float kts, char* buf, size_t len) {
    return fmt_speed(kts / 1.94384f, buf, len);
}

// ── Depth  (SI input: metres) ────────────────────────────────
inline const char* fmt_depth(float m, char* buf, size_t len) {
    if (isnan(m)) { snprintf(buf, len, "---"); return "m"; }
    switch (gSettings.depth_unit) {
        case DepthUnit::FEET:
            snprintf(buf, len, "%.1f", m * 3.28084f);
            return "ft";
        default: // METERS
            snprintf(buf, len, "%.1f", m);
            return "m";
    }
}

// ── Temperature  (SI input: °C) ──────────────────────────────
inline const char* fmt_temp(float c, char* buf, size_t len) {
    if (isnan(c)) { snprintf(buf, len, "---"); return "°C"; }
    switch (gSettings.temp_unit) {
        case TempUnit::FAHRENHEIT:
            snprintf(buf, len, "%.1f", c * 9.0f / 5.0f + 32.0f);
            return "°F";
        default: // CELSIUS
            snprintf(buf, len, "%.1f", c);
            return "°C";
    }
}

// ── Pressure  (SI input: kPa) ────────────────────────────────
inline const char* fmt_pressure(float kpa, char* buf, size_t len) {
    if (isnan(kpa)) { snprintf(buf, len, "---"); return "kPa"; }
    switch (gSettings.press_unit) {
        case PressUnit::PSI:
            snprintf(buf, len, "%.1f", kpa * 0.14504f);
            return "psi";
        default: // KPA
            snprintf(buf, len, "%.1f", kpa);
            return "kPa";
    }
}

// ── Angle / Heading  (no unit conversion, always degrees) ────
inline void fmt_angle(float deg, char* buf, size_t len) {
    if (isnan(deg)) snprintf(buf, len, "---°");
    else            snprintf(buf, len, "%.0f°", deg);
}

inline void fmt_angle_signed(float deg, char* buf, size_t len) {
    if (isnan(deg)) snprintf(buf, len, "---°");
    else            snprintf(buf, len, "%+.0f°", deg);
}

// ── Voltage  (no conversion) ─────────────────────────────────
inline void fmt_volts(float v, char* buf, size_t len) {
    if (isnan(v)) snprintf(buf, len, "---");
    else          snprintf(buf, len, "%.2f", v);
}

// ── RPM  (no conversion) ─────────────────────────────────────
inline void fmt_rpm(float rpm, char* buf, size_t len) {
    if (isnan(rpm)) snprintf(buf, len, "---");
    else            snprintf(buf, len, "%.0f", rpm);
}

// ── Threshold display: convert SI alarm threshold for display ─
// Returns value converted to current display unit
inline float alarm_threshold_display(AlarmID id, float si_val) {
    if (isnan(si_val)) return si_val;
    switch (id) {
        case ALARM_DEPTH_MIN:
        case ALARM_DEPTH_MAX:
            return (gSettings.depth_unit == DepthUnit::FEET)
                   ? si_val * 3.28084f : si_val;
        case ALARM_WIND_SPEED_MAX:
            switch (gSettings.speed_unit) {
                case SpeedUnit::KMH:  return si_val * 3.6f;
                case SpeedUnit::MPH:  return si_val * 2.23694f;
                default:              return si_val * 1.94384f; // knots
            }
        default: return si_val;
    }
}

// Convert display unit value back to SI for storage
inline float alarm_threshold_to_si(AlarmID id, float display_val) {
    if (isnan(display_val)) return display_val;
    switch (id) {
        case ALARM_DEPTH_MIN:
        case ALARM_DEPTH_MAX:
            return (gSettings.depth_unit == DepthUnit::FEET)
                   ? display_val / 3.28084f : display_val;
        case ALARM_WIND_SPEED_MAX:
            switch (gSettings.speed_unit) {
                case SpeedUnit::KMH:  return display_val / 3.6f;
                case SpeedUnit::MPH:  return display_val / 2.23694f;
                default:              return display_val / 1.94384f;
            }
        default: return display_val;
    }
}

// Unit label for alarm threshold spinbox
inline const char* alarm_threshold_unit(AlarmID id) {
    switch (id) {
        case ALARM_DEPTH_MIN:
        case ALARM_DEPTH_MAX:
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
