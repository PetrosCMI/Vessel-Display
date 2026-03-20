#include "settings.h"
#include <Preferences.h>

Settings gSettings;

// Alarm table — SI threshold units (metres, m/s)
const AlarmDef ALARM_TABLE[ALARM_COUNT] = {
    { ALARM_DEPTH_MIN,      "Depth MIN",       3.0f,  NAN   },
    { ALARM_DEPTH_MAX,      "Depth MAX",       NAN,   20.0f },
    { ALARM_WIND_SPEED_MAX, "Wind Speed MAX",  NAN,   15.4f },  // ~30 kts
};

static Preferences prefs;
static const char* NVS_NS = "boatdisp";

void settings_init() {
    prefs.begin(NVS_NS, false);

    // Version guard — if schema changes, wipe & use defaults
    uint8_t ver = prefs.getUChar("ver", 0);
    if (ver != 1) {
        Serial.println("[Settings] First boot or schema change — using defaults");
        prefs.clear();
        settings_save();
        return;
    }

    // Units
    gSettings.speed_unit  = (SpeedUnit) prefs.getUChar("speed_unit",  0);
    gSettings.depth_unit  = (DepthUnit) prefs.getUChar("depth_unit",  0);
    gSettings.temp_unit   = (TempUnit)  prefs.getUChar("temp_unit",   0);
    gSettings.press_unit  = (PressUnit) prefs.getUChar("press_unit",  0);

    // Audio / display
    gSettings.alarm_volume  = prefs.getUChar("vol",        200);
    gSettings.audio_enabled = prefs.getBool ("audio_en",   true);
    gSettings.brightness    = prefs.getUChar("brightness", 200);

    // Alarms — use fixed-size blob for the arrays
    prefs.getBytes("alarm_en",  gSettings.alarm_enabled, ALARM_COUNT);
    prefs.getBytes("alarm_lo",  gSettings.alarm_lo,      ALARM_COUNT * sizeof(float));
    prefs.getBytes("alarm_hi",  gSettings.alarm_hi,      ALARM_COUNT * sizeof(float));

    Serial.println("[Settings] Loaded from NVS");
    prefs.end();
}

void settings_save() {
    prefs.begin(NVS_NS, false);
    prefs.putUChar("ver",        1);
    prefs.putUChar("speed_unit", (uint8_t)gSettings.speed_unit);
    prefs.putUChar("depth_unit", (uint8_t)gSettings.depth_unit);
    prefs.putUChar("temp_unit",  (uint8_t)gSettings.temp_unit);
    prefs.putUChar("press_unit", (uint8_t)gSettings.press_unit);
    prefs.putUChar("vol",        gSettings.alarm_volume);
    prefs.putBool ("audio_en",   gSettings.audio_enabled);
    prefs.putUChar("brightness", gSettings.brightness);
    prefs.putBytes("alarm_en",   gSettings.alarm_enabled, ALARM_COUNT);
    prefs.putBytes("alarm_lo",   gSettings.alarm_lo,      ALARM_COUNT * sizeof(float));
    prefs.putBytes("alarm_hi",   gSettings.alarm_hi,      ALARM_COUNT * sizeof(float));
    prefs.end();
    Serial.println("[Settings] Saved to NVS");
}
