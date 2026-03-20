#pragma once
#include <Arduino.h>
#include "settings.h"

// ─────────────────────────────────────────────────────────────
//  ALARM ENGINE
// ─────────────────────────────────────────────────────────────

// Tone patterns for different alarm severities
enum class AlarmPattern : uint8_t {
    BEEP_ONCE,       // single 200ms beep — info
    BEEP_DOUBLE,     // two quick beeps — caution
    BEEP_TRIPLE,     // three beeps — warning
    CONTINUOUS,      // repeated beeps — critical (e.g. shallow water)
    SILENCE,         // cancel active alarm tone
};

struct ToneCmd {
    uint32_t      freq_hz;
    uint32_t      duration_ms;
    AlarmPattern  pattern;
};

// Call once from setup() — starts I2S and alarm FreeRTOS task
void alarm_init();

// Call every second from main loop — evaluates all conditions
void alarm_tick();

// Manually silence all active alarms (e.g. user presses mute on screen)
void alarm_silence();

// Returns true if any alarm is currently active
bool alarm_any_active();

// Returns true for a specific alarm
bool alarm_is_active(AlarmID id);

// Queue a tone directly (used by UI for confirmation beeps etc.)
void alarm_beep(AlarmPattern pattern);

// I2S pin config (set in config.h, read here)
// BCLK, LRCLK, DOUT
