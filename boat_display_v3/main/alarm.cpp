#include "alarm.h"
#include "boat_data.h"
#include "settings.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <math.h>
#include <string.h>

static const char* TAG = "Alarm";

// ─────────────────────────────────────────────
//  Tone synthesis via esp_codec_dev
//  The ES8311 is a proper audio codec — we write
//  PCM samples to it via esp_codec_dev_write().
// ─────────────────────────────────────────────
#define SAMPLE_RATE     22050
#define BYTES_PER_FRAME 2       // 16-bit mono

static esp_codec_dev_handle_t s_speaker = NULL;

// Generate and play a sine tone
static void play_tone(uint32_t freq_hz, uint32_t duration_ms) {
    if (!s_speaker || !gSettings.audio_enabled || freq_hz == 0) return;

    const uint32_t total_samples = (SAMPLE_RATE * duration_ms) / 1000;
    const float    scale         = (gSettings.alarm_volume / 100.0f) * 28000.0f;
    const size_t   buf_samples   = 512;
    int16_t        buf[buf_samples];

    uint32_t written = 0;
    float phase = 0.0f;
    const float phase_inc = 2.0f * (float)M_PI * freq_hz / SAMPLE_RATE;

    while (written < total_samples) {
        uint32_t chunk = buf_samples;
        if (chunk > total_samples - written) chunk = total_samples - written;
        for (uint32_t i = 0; i < chunk; i++) {
            buf[i] = (int16_t)(sinf(phase) * scale);
            phase += phase_inc;
            if (phase > 2.0f * (float)M_PI) phase -= 2.0f * (float)M_PI;
        }
        esp_codec_dev_write(s_speaker, buf, chunk * BYTES_PER_FRAME);
        written += chunk;
    }
}

static void play_silence(uint32_t duration_ms) {
    if (!s_speaker) return;
    const uint32_t total_samples = (SAMPLE_RATE * duration_ms) / 1000;
    int16_t buf[512] = {};
    uint32_t written = 0;
    while (written < total_samples) {
        uint32_t chunk = 512;
        if (chunk > total_samples - written) chunk = total_samples - written;
        esp_codec_dev_write(s_speaker, buf, chunk * BYTES_PER_FRAME);
        written += chunk;
    }
}

// ─────────────────────────────────────────────
//  Tone task
// ─────────────────────────────────────────────
struct ToneCmd { AlarmPattern pattern; };
static QueueHandle_t s_tone_queue = NULL;

static void tone_task(void* arg) {
    ToneCmd cmd;
    for (;;) {
        if (xQueueReceive(s_tone_queue, &cmd, portMAX_DELAY) == pdTRUE) {
            switch (cmd.pattern) {
                case AlarmPattern::BEEP_ONCE:
                    play_tone(880, 200);
                    break;
                case AlarmPattern::BEEP_DOUBLE:
                    play_tone(880, 150); play_silence(80); play_tone(880, 150);
                    break;
                case AlarmPattern::BEEP_TRIPLE:
                    play_tone(880, 120); play_silence(60);
                    play_tone(880, 120); play_silence(60);
                    play_tone(880, 120);
                    break;
                case AlarmPattern::CONTINUOUS:
                    for (int i = 0; i < 5; i++) {
                        play_tone(880, 300); play_silence(150);
                        play_tone(660, 200); play_silence(150);
                    }
                    break;
                case AlarmPattern::SILENCE:
                    // drain any pending tones
                    xQueueReset(s_tone_queue);
                    play_silence(50);
                    break;
            }
        }
    }
}

// ─────────────────────────────────────────────
//  Alarm state
// ─────────────────────────────────────────────
static bool    s_active[ALARM_COUNT]   = {};
static bool    s_silenced[ALARM_COUNT] = {};
static uint32_t s_last_tone_s[ALARM_COUNT] = {};
static bool    s_any_active = false;

static const uint32_t REPEAT_INTERVAL_S = 30;

// ─────────────────────────────────────────────
//  Public API
// ─────────────────────────────────────────────
void alarm_init(esp_codec_dev_handle_t speaker) {
    s_speaker = speaker;

    if (s_speaker) {
        esp_codec_dev_sample_info_t fs = {
            .bits_per_sample = 16,
            .channel         = 1,
            .sample_rate     = SAMPLE_RATE,
        };
        esp_codec_dev_set_out_vol(s_speaker, gSettings.alarm_volume);
        esp_codec_dev_open(s_speaker, &fs);
        ESP_LOGI(TAG, "Audio codec opened at %d Hz", SAMPLE_RATE);
    } else {
        ESP_LOGW(TAG, "No speaker handle — audio disabled");
    }

    s_tone_queue = xQueueCreate(8, sizeof(ToneCmd));
    xTaskCreatePinnedToCore(tone_task, "tone", 4096, NULL, 5, NULL, 0);
    ESP_LOGI(TAG, "Alarm engine started");
}

void alarm_beep(AlarmPattern pattern) {
    if (!s_tone_queue) return;
    ToneCmd cmd = { pattern };
    xQueueSend(s_tone_queue, &cmd, 0);
}

void alarm_silence(void) {
    for (int i = 0; i < ALARM_COUNT; i++) s_silenced[i] = true;
    alarm_beep(AlarmPattern::SILENCE);
}

bool alarm_any_active(void)           { return s_any_active; }
bool alarm_is_active(AlarmID id)      { return s_active[id]; }

static bool evaluate(AlarmID id, float si_value) {
    if (!gSettings.alarm_enabled[id] || isnan(si_value)) {
        s_active[id] = false;
        return false;
    }
    float lo = gSettings.alarm_lo[id];
    float hi = gSettings.alarm_hi[id];
    bool triggered = (!isnan(lo) && si_value < lo) ||
                     (!isnan(hi) && si_value > hi);
    if (!triggered) s_silenced[id] = false; // re-arm when condition clears
    s_active[id] = triggered;
    return triggered;
}

void alarm_tick(void) {
    BoatData d = boatDataSnapshot();
    uint32_t now_s = xTaskGetTickCount() * portTICK_PERIOD_MS / 1000;

    evaluate(ALARM_DEPTH_MIN,      d.depth_m);
    evaluate(ALARM_DEPTH_MAX,      d.depth_m);
    evaluate(ALARM_WIND_SPEED_MAX, d.aws_ms);   // already in m/s

    s_any_active = false;
    for (int i = 0; i < ALARM_COUNT; i++) {
        if (!s_active[i]) continue;
        s_any_active = true;
        if (s_silenced[i]) continue;
        if (now_s - s_last_tone_s[i] < REPEAT_INTERVAL_S) continue;
        s_last_tone_s[i] = now_s;

        AlarmPattern pat = (i == ALARM_DEPTH_MIN)
            ? AlarmPattern::CONTINUOUS
            : AlarmPattern::BEEP_TRIPLE;
        alarm_beep(pat);
    }
}
