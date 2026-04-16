#pragma once
#include <Arduino.h>

// ─────────────────────────────────────────────
//  Shared boat data  (updated by SignalK task,
//  read by LVGL UI task — protected by mutex)
// ─────────────────────────────────────────────

struct BoatData {
    // Navigation
    float sog_kts      = NAN;   // Speed over ground (knots)
    float stw_kts      = NAN;   // Speed through water (knots)
    float cog_deg      = NAN;   // Course over ground (°T)
    float heading_deg  = NAN;   // Magnetic / true heading (°)
    float lat          = NAN;   // GPS latitude
    float lon          = NAN;   // GPS longitude
    bool  gps_valid    = false;

    // Wind
    float aws_kts      = NAN;   // Apparent wind speed (knots)
    float awa_deg      = NAN;   // Apparent wind angle (°)
    float tws_kts      = NAN;   // True wind speed (knots)
    float twa_deg      = NAN;   // True wind angle (°)
    float twd_deg      = NAN;   // True wind direction (°T)

    // Depth / environment
    float depth_m      = NAN;   // Depth below transducer (m)
    float water_temp_c = NAN;   // Water temperature (°C)
    float air_temp_c   = NAN;

    // Engine / electrical
    float rpm          = NAN;   // Engine RPM
    float battery_v    = NAN;   // House bank voltage
    float alt_v        = NAN;   // Alternator / start bank
    float coolant_temp = NAN;   // Engine coolant temp (°C)
    float oil_pressure = NAN;   // Oil pressure (kPa)

    // Meta
    unsigned long last_update_ms = 0;
    bool signalk_connected = false;
};

extern BoatData gBoat;
extern SemaphoreHandle_t gBoatMutex;

// Helper: safely copy boat data for UI reads
inline BoatData boatDataSnapshot() {
    BoatData snap;
    if (xSemaphoreTake(gBoatMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        snap = gBoat;
        xSemaphoreGive(gBoatMutex);
    }
    return snap;
}

// Helper: write a single float field under mutex
inline void boatSet(float& field, float value) {
    if (xSemaphoreTake(gBoatMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        field = value;
        gBoat.last_update_ms = millis();
        xSemaphoreGive(gBoatMutex);
    }
}
