#pragma once
#include <stdint.h>
#include <string.h>
#include <math.h>

enum class SpeedUnit  : uint8_t { KNOTS = 0, KMH, MPH };
enum class DepthUnit  : uint8_t { METERS = 0, FEET };
enum class TempUnit   : uint8_t { CELSIUS = 0, FAHRENHEIT };
enum class PressUnit  : uint8_t { KPA = 0, PSI };

enum AlarmID : uint8_t {
    ALARM_DEPTH_MIN = 0,
    ALARM_DEPTH_MAX,
    ALARM_WIND_SPEED_MAX,
    ALARM_BATT_HOUSE,
    ALARM_BATT_START,
    ALARM_BATT_FORWARD,
    ALARM_ANCHOR_DRAG,
    ALARM_STORM,
    ALARM_DATA_TIMEOUT,    // No MQTT data for 5 minutes
    ALARM_COUNT
};

struct AlarmDef {
    AlarmID     id;
    const char* label;
    float       default_lo;
    float       default_hi;
};

extern const AlarmDef ALARM_TABLE[ALARM_COUNT];

// ΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇ
//  WiFi network slot
// ΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇ
#define WIFI_MAX_NETWORKS  5
#define WIFI_SSID_LEN     33   // 32 chars + null
#define WIFI_PASS_LEN     65   // 64 chars + null

struct WifiNetwork {
    char ssid[WIFI_SSID_LEN]  = {};
    char pass[WIFI_PASS_LEN]  = {};
    bool enabled              = false;
};

// ΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇ
//  SignalK connection
// ΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇ
#define SIGNALK_HOST_LEN  64
#define SIGNALK_PORT_MIN  1
#define SIGNALK_PORT_MAX  65535

//  MQTT connection
#define MQTT_HOST_LEN  64
#define MQTT_PORT_MIN  1
#define MQTT_PORT_MAX  65535

// ΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇ
//  Master settings struct
// ΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇ
struct Settings {
    // Units
    SpeedUnit  speed_unit  = SpeedUnit::KNOTS;
    DepthUnit  depth_unit  = DepthUnit::METERS;
    TempUnit   temp_unit   = TempUnit::CELSIUS;
    PressUnit  press_unit  = PressUnit::KPA;

    // Alarms — depth, wind, 3× battery low, anchor drag, storm, data timeout
    bool  alarm_enabled[ALARM_COUNT] = { true,  false, true,  true,  true,  true,  false, true,  true  };
    float alarm_lo[ALARM_COUNT]      = { 3.0f,  NAN,   NAN,   12.0f, 12.0f, 12.0f, NAN,   NAN,   NAN  };
    float alarm_hi[ALARM_COUNT]      = { NAN,   20.0f, 15.4f, NAN,   NAN,   NAN,   NAN,   NAN,   NAN  };

    // Shared battery low voltage threshold
    float battery_low_v = 12.0f;

    // Anchor watch
    float anchor_radius_m = 50.0f;  // drag alarm radius in metres

    // Audio / display
    uint8_t alarm_volume   = 70;
    bool    audio_enabled  = true;
    int     brightness_pct = 80;
    uint8_t alarm_rearm_minutes     = 5;
    uint8_t display_timeout_minutes = 10;  // dim after N minutes, off after N+1

    // WiFi networks ΓÇö index 0 = highest priority
    WifiNetwork wifi[WIFI_MAX_NETWORKS];

    // SignalK (legacy - kept for compatibility)
    //char signalk_host[SIGNALK_HOST_LEN] = "192.168.1.100";
    //uint16_t signalk_port               = 3000;
    
    // MQTT broker
    char mqtt_host[SIGNALK_HOST_LEN] = "192.168.1.100";
    uint16_t mqtt_port               = 1883;
};

extern Settings gSettings;

void settings_init();
void settings_save();

// ΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇ
//  Network reconnect ΓÇö call after changing
//  WiFi or SignalK settings
// ΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇ
void network_reconnect(void);
