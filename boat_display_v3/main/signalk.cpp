#include "signalk.h"
#include "signalk_paths.h"
#include "boat_data.h"
#include "settings.h"
#include "esp_websocket_client.h"
#include "esp_log.h"
#include "cJSON.h"
#include <string.h>
#include <math.h>

static const char* TAG = "SignalK";
static esp_websocket_client_handle_t s_client = NULL;

static inline float rad2deg(float r) { return r * 57.29578f; }
static inline float K2C(float k)     { return k - 273.15f;   }

// ΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇ
//  Delta parser ΓÇö unchanged
// ΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇ
static void parse_delta(const char* data, int len) {
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

            if (strcmp(path, SK_SOG) == 0)
                boatSet(gBoat.sog_ms, (float)value->valuedouble);
            else if (strcmp(path, SK_STW) == 0)
                boatSet(gBoat.stw_ms, (float)value->valuedouble);
            else if (strcmp(path, SK_COG) == 0)
                boatSet(gBoat.cog_deg, rad2deg((float)value->valuedouble));
            else if (strcmp(path, SK_HDG_TRUE) == 0)
                boatSet(gBoat.heading_deg, rad2deg((float)value->valuedouble));
            else if (strcmp(path, SK_HDG_MAG) == 0) {
                if (xSemaphoreTake(gBoatMutex, pdMS_TO_TICKS(5)) == pdTRUE) {
                    if (isnan(gBoat.heading_deg))
                        gBoat.heading_deg = rad2deg((float)value->valuedouble);
                    xSemaphoreGive(gBoatMutex);
                }
            }
            else if (strcmp(path, SK_POSITION) == 0) {
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
            else if (strcmp(path, SK_AWS) == 0) {
                float aws = (float)value->valuedouble;
                if (xSemaphoreTake(gBoatMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                    gBoat.aws_ms = aws;
                    if (isnan(gBoat.aws_max_ms) || aws > gBoat.aws_max_ms)
                        gBoat.aws_max_ms = aws;
                    gBoat.last_update_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;
                    xSemaphoreGive(gBoatMutex);
                }
            }
            else if (strcmp(path, SK_AWA) == 0)
                boatSet(gBoat.awa_deg, rad2deg((float)value->valuedouble));
            else if (strcmp(path, SK_TWS) == 0)
                boatSet(gBoat.tws_ms, (float)value->valuedouble);
            else if (strcmp(path, SK_TWA) == 0)
                boatSet(gBoat.twa_deg, rad2deg((float)value->valuedouble));
            else if (strcmp(path, SK_TWD) == 0)
                boatSet(gBoat.twd_deg, rad2deg((float)value->valuedouble));
            else if (strcmp(path, SK_DEPTH) == 0) {
                float depth = (float)value->valuedouble;
                if (xSemaphoreTake(gBoatMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                    gBoat.depth_m = depth;
                    if (isnan(gBoat.depth_max_m) || depth > gBoat.depth_max_m)
                        gBoat.depth_max_m = depth;
                    if (isnan(gBoat.depth_min_m) || depth < gBoat.depth_min_m)
                        gBoat.depth_min_m = depth;
                    gBoat.last_update_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;
                    xSemaphoreGive(gBoatMutex);
                }
            }
            else if (strcmp(path, SK_WATER_TEMP) == 0)
                boatSet(gBoat.water_temp_c, K2C((float)value->valuedouble));
            else if (strcmp(path, SK_AIR_TEMP) == 0)
                boatSet(gBoat.air_temp_c, K2C((float)value->valuedouble));
            else if (strcmp(path, SK_PRESSURE) == 0)
                boatSet(gBoat.pressure_hpa, (float)value->valuedouble / 100.0f); // Pa ΓåÆ hPa
            else if (strcmp(path, SK_STORM_LEVEL) == 0)
                boatSet(gBoat.storm_level, (float)value->valuedouble);
            else if (strcmp(path, SK_RPM) == 0)
                boatSet(gBoat.rpm, (float)value->valuedouble * 60.0f);
            else if (strcmp(path, SK_COOLANT_TEMP) == 0)
                boatSet(gBoat.coolant_temp, K2C((float)value->valuedouble));
            else if (strcmp(path, SK_OIL_PRESSURE) == 0)
                boatSet(gBoat.oil_pressure, (float)value->valuedouble / 1000.0f);
            else if (strcmp(path, SK_BATT_HOUSE_V) == 0)
                boatSet(gBoat.house_v, (float)value->valuedouble);
            else if (strcmp(path, SK_BATT_HOUSE_A_ALL) == 0)
                boatSet(gBoat.house_a_all, (float)value->valuedouble);
            else if (strcmp(path, SK_BATT_HOUSE_A_LI) == 0)
                boatSet(gBoat.house_a_li, (float)value->valuedouble);
            else if (strcmp(path, SK_BATT_START_V) == 0)
                boatSet(gBoat.start_batt_v, (float)value->valuedouble);
            else if (strcmp(path, SK_BATT_START_A) == 0)
                boatSet(gBoat.start_batt_a, (float)value->valuedouble);
            else if (strcmp(path, SK_BATT_FWD_V) == 0)
                boatSet(gBoat.forward_v, (float)value->valuedouble);
        }
    }
    cJSON_Delete(root);
}

// ΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇ
//  WebSocket event handler
// ΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇ
static void ws_event_handler(void* handler_args,
                              esp_event_base_t base,
                              int32_t event_id,
                              void* event_data) {
    esp_websocket_event_data_t* data = (esp_websocket_event_data_t*)event_data;
    switch (event_id) {
        case WEBSOCKET_EVENT_CONNECTED:
            ESP_LOGI(TAG, "Connected");
            if (xSemaphoreTake(gBoatMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                gBoat.signalk_connected = true;
                xSemaphoreGive(gBoatMutex);
            }
            // Spawn a task to send subscription ΓÇö avoids blocking the
            // websocket event handler which can corrupt client state
            xTaskCreate([](void*) {
                vTaskDelay(pdMS_TO_TICKS(200));
                if (s_client && esp_websocket_client_is_connected(s_client)) {
                    esp_websocket_client_send_text(s_client,
                        "{\"context\":\"vessels.self\","
                        "\"subscribe\":[{\"path\":\"*\",\"period\":1000}]}",
                        -1, pdMS_TO_TICKS(2000));
                    ESP_LOGI(TAG, "Subscription sent");
                }
                vTaskDelete(NULL);
            }, "sk_sub", 4096, NULL, 5, NULL);
            break;
        case WEBSOCKET_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "Disconnected");
            if (xSemaphoreTake(gBoatMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                gBoat.signalk_connected = false;
                xSemaphoreGive(gBoatMutex);
            }
            break;
        case WEBSOCKET_EVENT_DATA:
            if (data->op_code == 0x01 && data->data_len > 0)
                parse_delta(data->data_ptr, data->data_len);
            break;
        case WEBSOCKET_EVENT_ERROR:
            ESP_LOGW(TAG, "WS error");
            break;
        default: break;
    }
}

// ΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇ
//  Public API
// ΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇ
void signalk_start(void) {
    // Use gSettings ΓÇö falls back to hardcoded defaults if NVS empty
    const char* host = gSettings.signalk_host[0]
                       ? gSettings.signalk_host : "192.168.1.100";
    uint16_t    port = gSettings.signalk_port
                       ? gSettings.signalk_port : 3000;

    char uri[128];
    snprintf(uri, sizeof(uri), "ws://%s:%d/signalk/v1/stream?subscribe=all",
             host, port);

    esp_websocket_client_config_t cfg = {};
    cfg.uri                  = uri;
    cfg.reconnect_timeout_ms = 3000;
    cfg.network_timeout_ms   = 5000;
    cfg.buffer_size          = 4096;
    cfg.task_stack            = 6144;

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
    if (xSemaphoreTake(gBoatMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        gBoat.signalk_connected = false;
        xSemaphoreGive(gBoatMutex);
    }
}
