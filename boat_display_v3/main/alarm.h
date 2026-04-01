#pragma once
#include "settings.h"
#include "esp_codec_dev.h"

enum class AlarmPattern : uint8_t {
    BEEP_ONCE,
    BEEP_DOUBLE,
    BEEP_TRIPLE,
    CONTINUOUS,
    SILENCE,
};

void alarm_init(esp_codec_dev_handle_t speaker);
void alarm_tick(void);

// Acknowledge all active alarms — re-arms after alarm_rearm_minutes
void alarm_acknowledge(void);
// Acknowledge a single alarm
void alarm_acknowledge_one(AlarmID id);

// Permanent silence until condition clears (all alarms)
void alarm_silence(void);
// Permanent silence for a single alarm
void alarm_silence_one(AlarmID id);

void alarm_beep(AlarmPattern pattern);

bool alarm_any_active(void);
bool alarm_any_unacked(void);
bool alarm_is_active(AlarmID id);
bool alarm_is_acked(AlarmID id);