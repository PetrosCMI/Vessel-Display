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

// Acknowledge all active alarms (call on touch)
// Silences beeping for alarm_rearm_minutes, then re-arms if condition persists
void alarm_acknowledge(void);

void alarm_silence(void);   // permanent silence until condition clears
void alarm_beep(AlarmPattern pattern);

bool alarm_any_active(void);
bool alarm_any_unacked(void);
bool alarm_is_active(AlarmID id);
bool alarm_is_acked(AlarmID id);
