#include "alarm.h"
#include "boat_data.h"
#include "settings.h"
#include "config.h"
#include <driver/i2s.h>
#include <math.h>

// ─────────────────────────────────────────────────────────────
//  I2S AUDIO CONFIG
// ─────────────────────────────────────────────────────────────
#define I2S_PORT        I2S_NUM_0
#define I2S_SAMPLE_RATE 16000
#define I2S_DMA_BUF_LEN 512
#define I2S_DMA_BUF_CNT 4

static bool i2s_ready = false;

static void i2s_init() {
    i2s_config_t cfg = {
        .mode                 = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
        .sample_rate          = I2S_SAMPLE_RATE,
        .bits_per_sample      = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format       = I2S_CHANNEL_FMT_ONLY_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags     = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count        = I2S_DMA_BUF_CNT,
        .dma_buf_len          = I2S_DMA_BUF_LEN,
        .use_apll             = false,
        .tx_desc_auto_clear   = true,
    };
    i2s_pin_config_t pins = {
        .bck_io_num   = I2S_BCLK_PIN,
        .ws_io_num    = I2S_LRCK_PIN,
        .data_out_num = I2S_DOUT_PIN,
        .data_in_num  = I2S_PIN_NO_CHANGE,
    };
    esp_err_t err = i2s_driver_install(I2S_PORT, &cfg, 0, NULL);
    if (err != ESP_OK) {
        Serial.printf("[Alarm] I2S install failed: %d\n", err);
        return;
    }
    err = i2s_set_pin(I2S_PORT, &pins);
    if (err != ESP_OK) {
        Serial.printf("[Alarm] I2S pin set failed: %d\n", err);
        return;
    }
    i2s_ready = true;
    Serial.println("[Alarm] I2S OK");
}

// ─────────────────────────────────────────────────────────────
//  TONE GENERATION  (sine wave into I2S buffer)
// ─────────────────────────────────────────────────────────────
static void play_tone(uint32_t freq_hz, uint32_t duration_ms, uint8_t volume) {
    if (!i2s_ready || !gSettings.audio_enabled) return;
    if (freq_hz == 0 || duration_ms == 0) return;

    const uint32_t samples = (I2S_SAMPLE_RATE * duration_ms) / 1000;
    const float    scale   = (volume / 255.0f) * 28000.0f;  // 16-bit headroom

    static int16_t buf[I2S_DMA_BUF_LEN];
    uint32_t written_total = 0;
    float phase = 0.0f;
    const float phase_inc = 2.0f * M_PI * freq_hz / I2S_SAMPLE_RATE;

    while (written_total < samples) {
        uint32_t chunk = min((uint32_t)I2S_DMA_BUF_LEN, samples - written_total);
        for (uint32_t i = 0; i < chunk; i++) {
            buf[i] = (int16_t)(sinf(phase) * scale);
            phase += phase_inc;
            if (phase > 2.0f * M_PI) phase -= 2.0f * M_PI;
        }
        size_t bytes_written = 0;
        i2s_write(I2S_PORT, buf, chunk * sizeof(int16_t), &bytes_written, portMAX_DELAY);
        written_total += chunk;
    }
}

static void play_silence(uint32_t duration_ms) {
    if (!i2s_ready) return;
    static int16_t silent[I2S_DMA_BUF_LEN] = {};
    uint32_t samples = (I2S_SAMPLE_RATE * duration_ms) / 1000;
    uint32_t written = 0;
    while (written < samples) {
        uint32_t chunk = min((uint32_t)I2S_DMA_BUF_LEN, samples - written);
        size_t bytes_written = 0;
        i2s_write(I2S_PORT, silent, chunk * sizeof(int16_t), &bytes_written, portMAX_DELAY);
        written += chunk;
    }
}

// ─────────────────────────────────────────────────────────────
//  TONE QUEUE & AUDIO TASK
// ─────────────────────────────────────────────────────────────
static QueueHandle_t tone_queue = NULL;

static void audio_task(void* arg) {
    ToneCmd cmd;
    for (;;) {
        if (xQueueReceive(tone_queue, &cmd, portMAX_DELAY) == pdTRUE) {
            uint8_t vol = gSettings.alarm_volume;
            switch (cmd.pattern) {
                case AlarmPattern::BEEP_ONCE:
                    play_tone(cmd.freq_hz, 200, vol);
                    break;
                case AlarmPattern::BEEP_DOUBLE:
                    play_tone(cmd.freq_hz, 150, vol);
                    play_silence(80);
                    play_tone(cmd.freq_hz, 150, vol);
                    break;
                case AlarmPattern::BEEP_TRIPLE:
                    play_tone(cmd.freq_hz, 120, vol);
                    play_silence(60);
                    play_tone(cmd.freq_hz, 120, vol);
                    play_silence(60);
                    play_tone(cmd.freq_hz, 120, vol);
                    break;
                case AlarmPattern::CONTINUOUS:
                    // Repeat urgent pattern 5 times
                    for (int i = 0; i < 5; i++) {
                        play_tone(880, 300, vol);
                        play_silence(150);
                        play_tone(660, 200, vol);
                        play_silence(150);
                    }
                    break;
                case AlarmPattern::SILENCE:
                    i2s_zero_dma_buffer(I2S_PORT);
                    break;
            }
        }
    }
}

// ─────────────────────────────────────────────────────────────
//  ALARM STATE
// ─────────────────────────────────────────────────────────────
static bool  alarm_active[ALARM_COUNT]  = {};
static bool  alarm_silenced[ALARM_COUNT] = {};
static bool  any_active = false;

// Minimum seconds between repeat alarm tones
static const uint32_t ALARM_REPEAT_INTERVAL_S = 30;
static uint32_t alarm_last_tone_s[ALARM_COUNT] = {};

// ─────────────────────────────────────────────────────────────
//  PUBLIC API
// ─────────────────────────────────────────────────────────────
void alarm_init() {
    i2s_init();
    tone_queue = xQueueCreate(8, sizeof(ToneCmd));
    xTaskCreatePinnedToCore(audio_task, "audio", 4096, NULL, 1, NULL, 0); // Core 0
    Serial.println("[Alarm] Engine started");
}

void alarm_beep(AlarmPattern pattern) {
    if (!tone_queue) return;
    ToneCmd cmd = { 880, 200, pattern };
    xQueueSend(tone_queue, &cmd, 0);  // non-blocking
}

void alarm_silence() {
    for (int i = 0; i < ALARM_COUNT; i++) alarm_silenced[i] = true;
    ToneCmd cmd = { 0, 0, AlarmPattern::SILENCE };
    if (tone_queue) xQueueSend(tone_queue, &cmd, 0);
}

bool alarm_any_active()           { return any_active; }
bool alarm_is_active(AlarmID id)  { return alarm_active[id]; }

// Evaluate one alarm — returns true if newly triggered this tick
static bool evaluate(AlarmID id, float value) {
    if (!gSettings.alarm_enabled[id]) {
        alarm_active[id] = false;
        return false;
    }
    if (isnan(value)) {
        alarm_active[id] = false;
        return false;
    }

    float lo = gSettings.alarm_lo[id];
    float hi = gSettings.alarm_hi[id];

    bool triggered = false;
    if (!isnan(lo) && value < lo) triggered = true;
    if (!isnan(hi) && value > hi) triggered = true;

    bool newly_active = triggered && !alarm_active[id];
    alarm_active[id] = triggered;

    if (!triggered) {
        alarm_silenced[id] = false;  // re-arm once condition clears
    }

    return newly_active;
}

void alarm_tick() {
    BoatData d = boatDataSnapshot();
    uint32_t now_s = millis() / 1000;

    any_active = false;

    // ── Evaluate each alarm ──────────────────────────────────
    evaluate(ALARM_DEPTH_MIN, d.depth_m);
    evaluate(ALARM_DEPTH_MAX, d.depth_m);
    evaluate(ALARM_WIND_SPEED_MAX, d.aws_kts / 1.94384f);  // kts → m/s

    // ── Fire tones for active, un-silenced alarms ────────────
    for (int i = 0; i < ALARM_COUNT; i++) {
        if (!alarm_active[i]) continue;
        any_active = true;
        if (alarm_silenced[i]) continue;

        // Rate-limit: don't re-trigger tone faster than interval
        if (now_s - alarm_last_tone_s[i] < ALARM_REPEAT_INTERVAL_S) continue;
        alarm_last_tone_s[i] = now_s;

        // Choose pattern by severity
        AlarmPattern pat;
        if (i == ALARM_DEPTH_MIN)  pat = AlarmPattern::CONTINUOUS;  // shallow water = critical
        else                        pat = AlarmPattern::BEEP_TRIPLE;

        alarm_beep(pat);
    }
}
