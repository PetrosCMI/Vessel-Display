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
    ALARM_COUNT
};

struct AlarmDef {
    AlarmID     id;
    const char* label;
    float       default_lo;
    float       default_hi;
};

extern const AlarmDef ALARM_TABLE[ALARM_COUNT];

// ---------------------------------------------
//  WiFi network slot
// ---------------------------------------------
#define WIFI_MAX_NETWORKS  5
#define WIFI_SSID_LEN     33   // 32 chars + null
#define WIFI_PASS_LEN     65   // 64 chars + null

struct WifiNetwork {
    char ssid[WIFI_SSID_LEN]  = {};
    char pass[WIFI_PASS_LEN]  = {};
    bool enabled              = false;
};

// ---------------------------------------------
//  SignalK connection
// ---------------------------------------------
#define SIGNALK_HOST_LEN  64
#define SIGNALK_PORT_MIN  1
#define SIGNALK_PORT_MAX  65535

// ---------------------------------------------
//  Master settings struct
// ---------------------------------------------
struct Settings {
    // Units
    SpeedUnit  speed_unit  = SpeedUnit::KNOTS;
    DepthUnit  depth_unit  = DepthUnit::METERS;
    TempUnit   temp_unit   = TempUnit::CELSIUS;
    PressUnit  press_unit  = PressUnit::KPA;

    // Alarms ΓÇö depth, wind, 3├ù battery low
    bool  alarm_enabled[ALARM_COUNT] = { true,  false, true,  true,  true,  true  };
    float alarm_lo[ALARM_COUNT]      = { 3.0f,  NAN,   NAN,   12.0f, 12.0f, 12.0f };
    float alarm_hi[ALARM_COUNT]      = { NAN,   20.0f, 15.4f, NAN,   NAN,   NAN   };

    // Shared battery low voltage threshold (used as label in SET page)
    float battery_low_v = 12.0f;

    // Audio / display
    uint8_t alarm_volume   = 70;
    bool    audio_enabled  = true;
    int     brightness_pct = 80;
    uint8_t alarm_rearm_minutes     = 5;
    uint8_t display_timeout_minutes = 10;  // dim after N minutes, off after N+1

    // WiFi networks ΓÇö index 0 = highest priority
    WifiNetwork wifi[WIFI_MAX_NETWORKS];

    // SignalK
    char signalk_host[SIGNALK_HOST_LEN] = "192.168.1.100";
    uint16_t signalk_port               = 3000;
};

extern Settings gSettings;

void settings_init();
void settings_save();

// ---------------------------------------------
//  Network reconnect ΓÇö call after changing
//  WiFi or SignalK settings
// ---------------------------------------------
void network_reconnect(void);
