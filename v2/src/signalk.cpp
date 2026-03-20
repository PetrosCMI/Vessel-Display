#include "signalk.h"
#include "boat_data.h"
#include "config.h"

#include <WiFi.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>

static WebSocketsClient ws;
static bool             ws_connected = false;

// ─────────────────────────────────────────────
//  Helpers: unit conversions
// ─────────────────────────────────────────────
static inline float msToKnots(float ms)  { return ms * 1.94384f; }
static inline float radToDeg(float r)    { return r * 57.29578f; }
static inline float kToC(float k)        { return k - 273.15f; }

// ─────────────────────────────────────────────
//  Parse a single SignalK delta update message
// ─────────────────────────────────────────────
static void parseDelta(const char* payload) {
    // We use a stack-allocated filter doc to avoid allocating for fields we
    // don't care about — keeps heap fragmentation low.
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, payload);
    if (err) return;

    JsonArray updates = doc["updates"];
    if (updates.isNull()) return;

    for (JsonObject update : updates) {
        JsonArray values = update["values"];
        if (values.isNull()) continue;

        for (JsonObject val : values) {
            const char* path = val["path"];
            if (!path) continue;
            float v = val["value"].as<float>();

            // ── Navigation ──────────────────────────────
            if      (strcmp(path, "navigation.speedOverGround") == 0)
                boatSet(gBoat.sog_kts, msToKnots(v));
            else if (strcmp(path, "navigation.speedThroughWater") == 0)
                boatSet(gBoat.stw_kts, msToKnots(v));
            else if (strcmp(path, "navigation.courseOverGroundTrue") == 0)
                boatSet(gBoat.cog_deg, radToDeg(v));
            else if (strcmp(path, "navigation.headingTrue") == 0)
                boatSet(gBoat.heading_deg, radToDeg(v));
            else if (strcmp(path, "navigation.headingMagnetic") == 0) {
                // Use magnetic if true not available
                if (xSemaphoreTake(gBoatMutex, 5) == pdTRUE) {
                    if (isnan(gBoat.heading_deg)) gBoat.heading_deg = radToDeg(v);
                    xSemaphoreGive(gBoatMutex);
                }
            }
            else if (strcmp(path, "navigation.position") == 0) {
                // position value is an object {latitude, longitude}
                JsonObject pos = val["value"];
                if (!pos.isNull()) {
                    if (xSemaphoreTake(gBoatMutex, 5) == pdTRUE) {
                        gBoat.lat = pos["latitude"].as<float>();
                        gBoat.lon = pos["longitude"].as<float>();
                        gBoat.gps_valid = true;
                        gBoat.last_update_ms = millis();
                        xSemaphoreGive(gBoatMutex);
                    }
                }
            }

            // ── Wind ────────────────────────────────────
            else if (strcmp(path, "environment.wind.speedApparent") == 0)
                boatSet(gBoat.aws_kts, msToKnots(v));
            else if (strcmp(path, "environment.wind.angleApparent") == 0)
                boatSet(gBoat.awa_deg, radToDeg(v));
            else if (strcmp(path, "environment.wind.speedTrue") == 0)
                boatSet(gBoat.tws_kts, msToKnots(v));
            else if (strcmp(path, "environment.wind.angleTrueWater") == 0)
                boatSet(gBoat.twa_deg, radToDeg(v));
            else if (strcmp(path, "environment.wind.directionTrue") == 0)
                boatSet(gBoat.twd_deg, radToDeg(v));

            // ── Depth / Environment ──────────────────────
            else if (strcmp(path, "environment.depth.belowTransducer") == 0)
                boatSet(gBoat.depth_m, v);
            else if (strcmp(path, "environment.water.temperature") == 0)
                boatSet(gBoat.water_temp_c, kToC(v));
            else if (strcmp(path, "environment.outside.temperature") == 0)
                boatSet(gBoat.air_temp_c, kToC(v));

            // ── Engine / Electrical ──────────────────────
            else if (strcmp(path, "propulsion.main.revolutions") == 0)
                boatSet(gBoat.rpm, v * 60.0f);   // rps → rpm
            else if (strcmp(path, "propulsion.main.temperature") == 0)
                boatSet(gBoat.coolant_temp, kToC(v));
            else if (strcmp(path, "propulsion.main.oilPressure") == 0)
                boatSet(gBoat.oil_pressure, v / 1000.0f);  // Pa → kPa
            else if (strcmp(path, "electrical.batteries.house.voltage") == 0)
                boatSet(gBoat.battery_v, v);
            else if (strcmp(path, "electrical.batteries.start.voltage") == 0)
                boatSet(gBoat.alt_v, v);
        }
    }
}

// ─────────────────────────────────────────────
//  WebSocket event handler
// ─────────────────────────────────────────────
static void wsEvent(WStype_t type, uint8_t* payload, size_t length) {
    switch (type) {
        case WStype_CONNECTED:
            Serial.println("[SK] WebSocket connected");
            ws_connected = true;
            if (xSemaphoreTake(gBoatMutex, 10) == pdTRUE) {
                gBoat.signalk_connected = true;
                xSemaphoreGive(gBoatMutex);
            }
            // Send subscription request for all paths
            ws.sendTXT("{\"context\":\"vessels.self\","
                        "\"subscribe\":[{\"path\":\"*\",\"period\":1000}]}");
            break;

        case WStype_DISCONNECTED:
            Serial.println("[SK] WebSocket disconnected");
            ws_connected = false;
            if (xSemaphoreTake(gBoatMutex, 10) == pdTRUE) {
                gBoat.signalk_connected = false;
                xSemaphoreGive(gBoatMutex);
            }
            break;

        case WStype_TEXT:
            parseDelta((const char*)payload);
            break;

        case WStype_ERROR:
            Serial.printf("[SK] WS error, length=%u\n", length);
            break;

        default:
            break;
    }
}

// ─────────────────────────────────────────────
//  Public API
// ─────────────────────────────────────────────
void signalk_start() {
    String path = "/signalk/v1/stream?subscribe=all";
    ws.begin(SIGNALK_HOST, SIGNALK_PORT, path.c_str());
    ws.onEvent(wsEvent);
    ws.setReconnectInterval(3000);
    Serial.printf("[SK] Connecting to ws://%s:%d%s\n",
                  SIGNALK_HOST, SIGNALK_PORT, path.c_str());
}

void signalk_loop() {
    ws.loop();
}
