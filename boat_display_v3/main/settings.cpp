#include "settings.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"

static const char* TAG = "Settings";
static const char* NVS_NS = "boatdisp";
static const uint8_t SCHEMA_VER = 4;   // bumped ΓÇö battery alarm fields added

Settings gSettings;

const AlarmDef ALARM_TABLE[ALARM_COUNT] = {
    { ALARM_DEPTH_MIN,      "Depth MIN",      3.0f,  NAN   },
    { ALARM_DEPTH_MAX,      "Depth MAX",      NAN,   20.0f },
    { ALARM_WIND_SPEED_MAX, "Wind Speed MAX", NAN,   15.4f },
    { ALARM_BATT_HOUSE,     "House Battery",  12.0f, NAN   },
    { ALARM_BATT_START,     "Start Battery",  12.0f, NAN   },
    { ALARM_BATT_FORWARD,   "Fwd Battery",    12.0f, NAN   },
};

void settings_init() {
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READONLY, &h) != ESP_OK) {
        ESP_LOGI(TAG, "No saved settings ΓÇö using defaults");
        settings_save();
        return;
    }

    uint8_t ver = 0;
    nvs_get_u8(h, "ver", &ver);
    if (ver != SCHEMA_VER) {
        ESP_LOGI(TAG, "Schema v%d ΓåÆ v%d ΓÇö resetting to defaults", ver, SCHEMA_VER);
        nvs_close(h);
        settings_save();
        return;
    }

    // Units
    uint8_t u8;
    if (nvs_get_u8(h, "speed_unit",  &u8) == ESP_OK) gSettings.speed_unit  = (SpeedUnit)u8;
    if (nvs_get_u8(h, "depth_unit",  &u8) == ESP_OK) gSettings.depth_unit  = (DepthUnit)u8;
    if (nvs_get_u8(h, "temp_unit",   &u8) == ESP_OK) gSettings.temp_unit   = (TempUnit)u8;
    if (nvs_get_u8(h, "press_unit",  &u8) == ESP_OK) gSettings.press_unit  = (PressUnit)u8;
    if (nvs_get_u8(h, "alarm_vol",   &u8) == ESP_OK) gSettings.alarm_volume = u8;
    if (nvs_get_u8(h, "rearm_min",  &u8) == ESP_OK) gSettings.alarm_rearm_minutes = u8;
    if (nvs_get_u8(h, "disp_tout",  &u8) == ESP_OK) gSettings.display_timeout_minutes = u8;

    uint8_t audio_en = 1;
    if (nvs_get_u8(h, "audio_en", &audio_en) == ESP_OK) gSettings.audio_enabled = audio_en;

    int32_t bright = 80;
    if (nvs_get_i32(h, "brightness", &bright) == ESP_OK) gSettings.brightness_pct = (int)bright;

    // Alarms
    size_t sz = sizeof(gSettings.alarm_enabled);
    nvs_get_blob(h, "alarm_en", gSettings.alarm_enabled, &sz);
    sz = sizeof(gSettings.alarm_lo);
    nvs_get_blob(h, "alarm_lo", gSettings.alarm_lo, &sz);
    sz = sizeof(gSettings.alarm_hi);
    nvs_get_blob(h, "alarm_hi", gSettings.alarm_hi, &sz);

    // Battery low threshold (stored as integer mV to avoid float NVS issues)
    uint32_t batt_mv = 12000;
    if (nvs_get_u32(h, "batt_low_mv", &batt_mv) == ESP_OK)
        gSettings.battery_low_v = batt_mv / 1000.0f;

    // WiFi networks
    sz = sizeof(gSettings.wifi);
    nvs_get_blob(h, "wifi_nets", &gSettings.wifi, &sz);

    // SignalK
    sz = sizeof(gSettings.signalk_host);
    nvs_get_str(h, "sk_host", gSettings.signalk_host, &sz);
    uint32_t port32 = 3000;
    if (nvs_get_u32(h, "sk_port", &port32) == ESP_OK)
        gSettings.signalk_port = (uint16_t)port32;

    nvs_close(h);
    ESP_LOGI(TAG, "Loaded from NVS (schema v%d)", SCHEMA_VER);
}

void settings_save() {
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READWRITE, &h) != ESP_OK) return;

    nvs_set_u8(h,  "ver",        SCHEMA_VER);
    nvs_set_u8(h,  "speed_unit", (uint8_t)gSettings.speed_unit);
    nvs_set_u8(h,  "depth_unit", (uint8_t)gSettings.depth_unit);
    nvs_set_u8(h,  "temp_unit",  (uint8_t)gSettings.temp_unit);
    nvs_set_u8(h,  "press_unit", (uint8_t)gSettings.press_unit);
    nvs_set_u8(h,  "alarm_vol",  gSettings.alarm_volume);
    nvs_set_u8(h,  "rearm_min",  gSettings.alarm_rearm_minutes);
    nvs_set_u8(h,  "disp_tout",  gSettings.display_timeout_minutes);
    nvs_set_u8(h,  "audio_en",   gSettings.audio_enabled ? 1 : 0);
    nvs_set_i32(h, "brightness", gSettings.brightness_pct);
    nvs_set_blob(h, "alarm_en",  gSettings.alarm_enabled, sizeof(gSettings.alarm_enabled));
    nvs_set_blob(h, "alarm_lo",  gSettings.alarm_lo,      sizeof(gSettings.alarm_lo));
    nvs_set_blob(h, "alarm_hi",  gSettings.alarm_hi,      sizeof(gSettings.alarm_hi));
    nvs_set_u32(h,  "batt_low_mv", (uint32_t)(gSettings.battery_low_v * 1000.0f));
    nvs_set_blob(h, "wifi_nets", &gSettings.wifi,         sizeof(gSettings.wifi));
    nvs_set_str(h,  "sk_host",   gSettings.signalk_host);
    nvs_set_u32(h,  "sk_port",   (uint32_t)gSettings.signalk_port);

    nvs_commit(h);
    nvs_close(h);
    ESP_LOGI(TAG, "Saved to NVS");
}
