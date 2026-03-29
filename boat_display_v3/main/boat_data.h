#pragma once
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <math.h>

// ΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇ
//  Shared boat data  (written by SignalK task, read by UI task)
//  All values in SI units:
//    speed   ΓåÆ m/s
//    depth   ΓåÆ metres
//    temp    ΓåÆ ┬░C
//    pressureΓåÆ kPa
//    angles  ΓåÆ degrees
// ΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇ
struct BoatData {
    // Navigation
    float sog_ms       = NAN;   // Speed over ground (m/s)
    float stw_ms       = NAN;   // Speed through water (m/s)
    float cog_deg      = NAN;   // Course over ground (┬░T)
    float heading_deg  = NAN;   // Heading true/magnetic (┬░)
    float lat          = NAN;
    float lon          = NAN;
    bool  gps_valid    = false;

    // Wind (SI: m/s, degrees)
    float aws_ms       = NAN;   // Apparent wind speed (m/s)
    float aws_max_ms   = NAN;   // session max AWS (reset by user)
    float awa_deg      = NAN;   // Apparent wind angle (┬░, signed ┬▒180)
    float tws_ms       = NAN;   // True wind speed (m/s)
    float twa_deg      = NAN;   // True wind angle (┬░, signed ┬▒180)
    float twd_deg      = NAN;   // True wind direction (┬░T)

    // Depth / environment
    float depth_m      = NAN;
    float depth_max_m  = NAN;   // session max depth (reset by user)
    float depth_min_m  = NAN;   // session min depth (reset by user)
    float water_temp_c = NAN;
    float air_temp_c   = NAN;
    float pressure_hpa = NAN;   // barometric pressure (hPa)
    float storm_level  = NAN;   // storm warning level (0=none, rising = worsening)

    // Engine / electrical
    float rpm          = NAN;
    float battery_v    = NAN;   // legacy
    float start_v      = NAN;   // legacy

    // Battery banks
    float house_v      = NAN; 
    float house_a      = NAN;   // house total current
    float house_v_li   = NAN;   // house LI voltage
    float house_a_li   = NAN;   // house LI current
    float start_batt_v = NAN;
    float start_batt_a = NAN;
    float forward_v    = NAN;

    float coolant_temp = NAN;
    float oil_pressure = NAN;

    // Anchor watch
    float    anchor_lat   = NAN;   // drop point latitude
    float    anchor_lon   = NAN;   // drop point longitude
    bool     anchor_set   = false; // anchor watch active
    float    anchor_dist_m = NAN;  // current distance from anchor (m)
    float    anchor_bearing_deg = NAN; // bearing from anchor to boat (┬░T)

    // Meta
    uint32_t last_update_ms    = 0;
    bool     signalk_connected = false;
};

extern BoatData          gBoat;
extern SemaphoreHandle_t gBoatMutex;

// Safe snapshot ΓÇö copies struct under mutex
inline BoatData boatDataSnapshot() {
    BoatData snap;
    if (xSemaphoreTake(gBoatMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        snap = gBoat;
        xSemaphoreGive(gBoatMutex);
    }
    return snap;
}

// Safe single-field write
inline void boatSet(float& field, float value) {
    if (xSemaphoreTake(gBoatMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        field = value;
        gBoat.last_update_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;
        xSemaphoreGive(gBoatMutex);
    }
}

// Reset depth min/max accumulators
inline void boatResetDepthMinMax() {
    if (xSemaphoreTake(gBoatMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        gBoat.depth_max_m = NAN;
        gBoat.depth_min_m = NAN;
        xSemaphoreGive(gBoatMutex);
    }
}

// Reset wind max accumulator
inline void boatResetWindMax() {
    if (xSemaphoreTake(gBoatMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        gBoat.aws_max_ms = NAN;
        xSemaphoreGive(gBoatMutex);
    }
}
