#include "signalk.h"
#include "boat_data.h"
#include "esp_websocket_client.h"
#include "esp_log.h"
#include "cJSON.h"
#include "config.h"
#include <string.h>
#include <math.h>

static const char* TAG = "SignalK";
static esp_websocket_client_handle_t s_client = NULL;

// ─────────────────────────────────────────────
//  Unit helpers (SignalK always sends SI)
// ─────────────────────────────────────────────
static inline float rad2deg(float r) { return r * 57.29578f; }
static inline float K2C(float k)     { return k - 273.15f;   }

// ─────────────────────────────────────────────
//  Delta parser
// ─────────────────────────────────────────────
static void parse_delta(const char* data, int len) {
    // Use a null-terminated copy for cJSON
    char* buf = (char*)malloc(len + 1);
    if (!buf) return;
    memcpy(buf, data, len);
    buf[len] = '\0';

    cJSON* root = cJSON_Parse(buf);
    free(buf);
    if (!root) return;

    cJSON* updates = cJSON_GetObjectItem(root, "updates");
    if (!cJSON_IsArray(updates)) { cJSON_Delete(root); return; }

    cJSON* update;
    cJSON_ArrayForEach(update, updates) {
        cJSON* values = cJSON_GetObjectItem(update, "values");
        if (!cJSON_IsArray(values)) continue;

        cJSON* val;
        cJSON_ArrayForEach(val, values) {
            const char* path = cJSON_GetStringValue(cJSON_GetObjectItem(val, "path"));
            if (!path) continue;
            cJSON* value = cJSON_GetObjectItem(val, "value");

            // ── Navigation ──────────────────────────────────
            if (strcmp(path, "navigation.speedOverGround") == 0)
                boatSet(gBoat.sog_ms, (float)value->valuedouble);
            else if (strcmp(path, "navigation.speedThroughWater") == 0)
                boatSet(gBoat.stw_ms, (float)value->valuedouble);
            else if (strcmp(path, "navigation.courseOverGroundTrue") == 0)
                boatSet(gBoat.cog_deg, rad2deg((float)value->valuedouble));
            else if (strcmp(path, "navigation.headingTrue") == 0)
                boatSet(gBoat.heading_deg, rad2deg((float)value->valuedouble));
            else if (strcmp(path, "navigation.headingMagnetic") == 0) {
                if (xSemaphoreTake(gBoatMutex, pdMS_TO_TICKS(5)) == pdTRUE) {
                    if (isnan(gBoat.heading_deg))
                        gBoat.heading_deg = rad2deg((float)value->valuedouble);
                    xSemaphoreGive(gBoatMutex);
                }
            }
            else if (strcmp(path, "navigation.position") == 0) {
                cJSON* lat = cJSON_GetObjectItem(value, "latitude");
                cJSON* lon = cJSON_GetObjectItem(value, "longitude");
                if (cJSON_IsNumber(lat) && cJSON_IsNumber(lon)) {
                    if (xSemaphoreTake(gBoatMutex, pdMS_TO_TICKS(5)) == pdTRUE) {
                        gBoat.lat = (float)lat->valuedouble;
                        gBoat.lon = (float)lon->valuedouble;
                        gBoat.gps_valid = true;
                        xSemaphoreGive(gBoatMutex);
                    }
                }
            }

            // ── Wind ────────────────────────────────────────
            else if (strcmp(path, "environment.wind.speedApparent") == 0)
                boatSet(gBoat.aws_ms, (float)value->valuedouble);
            else if (strcmp(path, "environment.wind.angleApparent") == 0)
                boatSet(gBoat.awa_deg, rad2deg((float)value->valuedouble));
            else if (strcmp(path, "environment.wind.speedTrue") == 0)
                boatSet(gBoat.tws_ms, (float)value->valuedouble);
            else if (strcmp(path, "environment.wind.angleTrueWater") == 0)
                boatSet(gBoat.twa_deg, rad2deg((float)value->valuedouble));
            else if (strcmp(path, "environment.wind.directionTrue") == 0)
                boatSet(gBoat.twd_deg, rad2deg((float)value->valuedouble));

            // ── Depth / Environment ─────────────────────────
            else if (strcmp(path, "environment.depth.belowTransducer") == 0)
                boatSet(gBoat.depth_m, (float)value->valuedouble);
            else if (strcmp(path, "environment.water.temperature") == 0)
                boatSet(gBoat.water_temp_c, K2C((float)value->valuedouble));
            else if (strcmp(path, "environment.outside.temperature") == 0)
                boatSet(gBoat.air_temp_c, K2C((float)value->valuedouble));

            // ── Engine / Electrical ─────────────────────────
            else if (strcmp(path, "propulsion.main.revolutions") == 0)
                boatSet(gBoat.rpm, (float)value->valuedouble * 60.0f); // rps → rpm
            else if (strcmp(path, "propulsion.main.temperature") == 0)
                boatSet(gBoat.coolant_temp, K2C((float)value->valuedouble));
            else if (strcmp(path, "propulsion.main.oilPressure") == 0)
                boatSet(gBoat.oil_pressure, (float)value->valuedouble / 1000.0f); // Pa → kPa
            else if (strcmp(path, "electrical.batteries.house.voltage") == 0)
                boatSet(gBoat.battery_v, (float)value->valuedouble);
            else if (strcmp(path, "electrical.batteries.start.voltage") == 0)
                boatSet(gBoat.start_v, (float)value->valuedouble);
        }
    }

    cJSON_Delete(root);
}

// ─────────────────────────────────────────────
//  WebSocket event handler
// ─────────────────────────────────────────────
static void ws_event_handler(void* handler_args,
                              esp_event_base_t base,
                              int32_t event_id,
                              void* event_data)
{
    esp_websocket_event_data_t* data = (esp_websocket_event_data_t*)event_data;

    switch (event_id) {
        case WEBSOCKET_EVENT_CONNECTED:
            ESP_LOGI(TAG, "Connected");
            if (xSemaphoreTake(gBoatMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                gBoat.signalk_connected = true;
                xSemaphoreGive(gBoatMutex);
            }
            // Subscribe to all paths at 1 Hz
            esp_websocket_client_send_text(s_client,
                "{\"context\":\"vessels.self\","
                "\"subscribe\":[{\"path\":\"*\",\"period\":1000}]}",
                -1, pdMS_TO_TICKS(1000));
            break;

        case WEBSOCKET_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "Disconnected");
            if (xSemaphoreTake(gBoatMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                gBoat.signalk_connected = false;
                xSemaphoreGive(gBoatMutex);
            }
            break;

        case WEBSOCKET_EVENT_DATA:
            if (data->op_code == 0x01 && data->data_len > 0) { // TEXT frame
                parse_delta(data->data_ptr, data->data_len);
            }
            break;

        case WEBSOCKET_EVENT_ERROR:
            ESP_LOGW(TAG, "WebSocket error");
            break;

        default:
            break;
    }
}

// ─────────────────────────────────────────────
//  Public API
// ─────────────────────────────────────────────
void signalk_start(void) {
    char uri[128];
    snprintf(uri, sizeof(uri), "ws://%s:%d/signalk/v1/stream?subscribe=all",
             SIGNALK_HOST, SIGNALK_PORT);

    esp_websocket_client_config_t cfg = {};
    cfg.uri                   = uri;
    cfg.reconnect_timeout_ms  = 3000;
    cfg.network_timeout_ms    = 5000;
    cfg.buffer_size           = 4096;
    cfg.task_stack             = 6144;

    s_client = esp_websocket_client_init(&cfg);
    esp_websocket_register_events(s_client, WEBSOCKET_EVENT_ANY,
                                  ws_event_handler, NULL);
    esp_websocket_client_start(s_client);

    ESP_LOGI(TAG, "Connecting to %s", uri);
}

void signalk_stop(void) {
    if (s_client) {
        esp_websocket_client_stop(s_client);
        esp_websocket_client_destroy(s_client);
        s_client = NULL;
    }
}
