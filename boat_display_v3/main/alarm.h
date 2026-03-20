#pragma once
#include "settings.h"
#include "esp_codec_dev.h"

enum class AlarmPattern : uint8_t {
    BEEP_ONCE,
    BEEP_DOUBLE,
    BEEP_TRIPLE,
    CONTINUOUS,   // shallow water critical
    SILENCE,
};

// Call once from app_main after bsp_audio_codec_speaker_init()
void alarm_init(esp_codec_dev_handle_t speaker);

// Evaluate all alarm conditions — call every second
void alarm_tick(void);

// Silence all active alarms
void alarm_silence(void);

// Queue a tone (non-blocking)
void alarm_beep(AlarmPattern pattern);

bool alarm_any_active(void);
bool alarm_is_active(AlarmID id);
