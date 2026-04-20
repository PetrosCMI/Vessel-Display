#include <mqtt_client.h>
#include "boat_data.h"
#include "settings.h"
#include "mqtt_client.h"
#include "esp_log.h"
#include "cJSON.h"
#include <string.h>
#include <math.h>

static const char* TAG = "MQTT";
static esp_mqtt_client_handle_t s_client = NULL;

static inline float rad2deg(float r) { return r * 57.29578f; }
static inline float K2C(float k)     { return k - 273.15f;   }

#define BI_MAX_TEMP_SENSORS 4
#define BI_MAX_CELLS 12

struct BatteryInfo {
    // Basic data
    float voltage_v = std::numeric_limits<float>::quiet_NaN();
    
    // Extended data
    float current_a = std::numeric_limits<float>::quiet_NaN();
    float soc_pct = std::numeric_limits<float>::quiet_NaN();
    float capacity_ah = std::numeric_limits<float>::quiet_NaN();

    // Arrays initialized to NaN
    float temp_c[BI_MAX_TEMP_SENSORS];
    float cell_v[BI_MAX_CELLS];
    
    uint32_t status_flags = 0; // Bitmask for errors/status

    // Constructor to ensure arrays are filled with NaN
    BatteryInfo() {
        for (int i = 0; i < BI_MAX_TEMP_SENSORS; i++) temp_c[i] = std::numeric_limits<float>::quiet_NaN();
        for (int i = 0; i < BI_MAX_CELLS; i++) cell_v[i] = std::numeric_limits<float>::quiet_NaN();
    }
};


// ────────────────────────────────────────────────
//  Parse MQTT Battery message payload
//  Example data
// vessels/PoleDancer/electrical/battery/house {"voltage":13.32333,"current":0.820167,"soc":100}
// vessels/PoleDancer/electrical/battery/starter {"voltage":12.71}
// vessels/PoleDancer/electrical/battery/house/li1 {"voltage":13.33,"current":0,"soc":81,"temp1":14.7,"temp2":14.3,"temp3":14.2,"cell1_v":3.332,"cell2_v":3.335,"cell3_v":3.334,"cell4_v":3.333,"capacity":242.41,"errors":""}
// vessels/PoleDancer/electrical/battery/forward {"voltage":13.0685}
// ────────────────────────────────────────────────
static BatteryInfo parse_battery(const char* data, int len) {
    BatteryInfo info; // All fields start as NaN

    // cJSON_Parse is null-terminated, but MQTT payloads usually aren't.
    // We create a temporary null-terminated string to be safe.
    char* buf = (char*)malloc(len + 1);
    if (!buf) return info;
    memcpy(buf, data, len);
    buf[len] = '\0';

    cJSON* root = cJSON_Parse(buf);
    if (!root) {
        // Fallback: If it's just a raw number (plain text voltage)
        float val = atof(buf);
        if (val != 0.0f || buf[0] == '0') {
            info.voltage_v = val;
        }
        free(buf);
        return info;
    }

    // Helper lambda to safely extract doubles
    auto set_if_present = [&](const char* key, float& target) {
        cJSON* item = cJSON_GetObjectItem(root, key);
        if (cJSON_IsNumber(item)) {
            target = (float)item->valuedouble;
        }
    };

    // Parse main fields
    set_if_present("voltage", info.voltage_v);
    set_if_present("current", info.current_a);
    set_if_present("soc",     info.soc_pct);
    set_if_present("capacity", info.capacity_ah);

    // Parse Temperature Array (temp1, temp2...)
    for (int i = 0; i < 4; i++) {
        char key[8];
        snprintf(key, sizeof(key), "temp%d", i + 1);
        set_if_present(key, info.temp_c[i]);
    }

    // Parse Cell Voltages (cell1_v, cell2_v...)
    for (int i = 0; i < 4; i++) {
        char key[10];
        snprintf(key, sizeof(key), "cell%d_v", i + 1);
        set_if_present(key, info.cell_v[i]);
    }

    cJSON_Delete(root);
    free(buf);
    return info;
}

// ────────────────────────────────────────────────
//  Parse MQTT message payload
//  SignalK publishes as JSON: {"value": 12.34}
//  Battery ESP32s publish as plain text: "12.34"
// ────────────────────────────────────────────────
static float parse_value(const char* data, int len) {
    if (!data || len <= 0) return NAN;
    
    // Try JSON parse first (SignalK format)
    char* buf = (char*)malloc(len + 1);
    if (!buf) return NAN;
    memcpy(buf, data, len);
    buf[len] = '\0';
    
    cJSON* root = cJSON_Parse(buf);
    if (root) {
        cJSON* value = cJSON_GetObjectItem(root, "value");
        float result = NAN;
        if (cJSON_IsNumber(value)) {
            result = (float)value->valuedouble;
        }
        cJSON_Delete(root);
        free(buf);
        return result;
    }
    
    // Fallback: plain text number
    float result = atof(buf);
    free(buf);
    return result;
}

// Parse position from SignalK JSON: {"value": {"latitude": 49.123, "longitude": -123.456}}
static bool parse_position(const char* data, int len, double* lat, double* lon) {
    if (!data || len <= 0) return false;
    
    char* buf = (char*)malloc(len + 1);
    if (!buf) return false;
    memcpy(buf, data, len);
    buf[len] = '\0';
    
    cJSON* root = cJSON_Parse(buf);
    if (!root) {
        free(buf);
        return false;
    }
    
    bool success = false;
    cJSON* value = cJSON_GetObjectItem(root, "value");
    if (value) {
        cJSON* lat_json = cJSON_GetObjectItem(value, "latitude");
        cJSON* lon_json = cJSON_GetObjectItem(value, "longitude");
        
        if (cJSON_IsNumber(lat_json) && cJSON_IsNumber(lon_json)) {
            *lat = lat_json->valuedouble;
            *lon = lon_json->valuedouble;
            success = true;
        }
    }
    
    cJSON_Delete(root);
    free(buf);
    return success;
}

// ────────────────────────────────────────────────
//  MQTT event handler
// ────────────────────────────────────────────────
static void mqtt_event_handler(void* handler_args, esp_event_base_t base,
                               int32_t event_id, void* event_data) {
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;
    
    ESP_LOGI(TAG, "mqtt_event_handler event_id: %d", event_id);
    switch (event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "Connected to broker");
            if (xSemaphoreTake(gBoatMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                gBoat.mqtt_connected = true;
                xSemaphoreGive(gBoatMutex);
            }
            
            // Subscribe to all topics
            esp_mqtt_client_subscribe(s_client, MQTT_SOG, 0);
            esp_mqtt_client_subscribe(s_client, MQTT_STW, 0);
            esp_mqtt_client_subscribe(s_client, MQTT_COG, 0);
            esp_mqtt_client_subscribe(s_client, MQTT_HDG_TRUE, 0);
            esp_mqtt_client_subscribe(s_client, MQTT_HDG_MAG, 0);
            esp_mqtt_client_subscribe(s_client, MQTT_POSITION, 0);
            esp_mqtt_client_subscribe(s_client, MQTT_DEPTH, 0);
            
            esp_mqtt_client_subscribe(s_client, MQTT_AWS, 0);
            ESP_LOGI( TAG, "Subscribed to %s", MQTT_AWS);
            esp_mqtt_client_subscribe(s_client, MQTT_AWA, 0);
            esp_mqtt_client_subscribe(s_client, MQTT_TWS, 0);
            esp_mqtt_client_subscribe(s_client, MQTT_TWA, 0);
            esp_mqtt_client_subscribe(s_client, MQTT_TWD, 0);
            
            esp_mqtt_client_subscribe(s_client, MQTT_WATER_TEMP, 0);
            esp_mqtt_client_subscribe(s_client, MQTT_AIR_TEMP, 0);
            esp_mqtt_client_subscribe(s_client, MQTT_PRESSURE, 0);
            
            // Battery topics
            esp_mqtt_client_subscribe(s_client, MQTT_BATT_HOUSE, 0);
            esp_mqtt_client_subscribe(s_client, MQTT_BATT_HOUSE_LI, 0);
            esp_mqtt_client_subscribe(s_client, MQTT_BATT_START, 0);
            esp_mqtt_client_subscribe(s_client, MQTT_BATT_FWD, 0);
            
            ESP_LOGI(TAG, "Subscribed to all topics");
            break;
            
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "Disconnected from broker");
            if (xSemaphoreTake(gBoatMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                gBoat.mqtt_connected = false;
                xSemaphoreGive(gBoatMutex);
            }
            break;
            
        case MQTT_EVENT_DATA: {
            char topic[128] = {};
            int topic_len = event->topic_len < 127 ? event->topic_len : 127;
            memcpy(topic, event->topic, topic_len);
            
            ESP_LOGI(TAG, "Topic: %s, Data: %.*s", topic, event->data_len, event->data);
            
            // Navigation
            if (strcmp(topic, MQTT_SOG) == 0)
                boatSet(gBoat.sog_ms, parse_value(event->data, event->data_len));
            else if (strcmp(topic, MQTT_STW) == 0)
                boatSet(gBoat.stw_ms, parse_value(event->data, event->data_len));
            else if (strcmp(topic, MQTT_COG) == 0)
                boatSet(gBoat.cog_deg, rad2deg(parse_value(event->data, event->data_len)));
            else if (strcmp(topic, MQTT_HDG_TRUE) == 0)
                boatSet(gBoat.heading_deg, rad2deg(parse_value(event->data, event->data_len)));
            else if (strcmp(topic, MQTT_HDG_MAG) == 0) {
                if (xSemaphoreTake(gBoatMutex, pdMS_TO_TICKS(5)) == pdTRUE) {
                    if (isnan(gBoat.heading_deg))
                        gBoat.heading_deg = rad2deg(parse_value(event->data, event->data_len));
                    xSemaphoreGive(gBoatMutex);
                }
            }
            else if (strcmp(topic, MQTT_POSITION) == 0) {
                double lat, lon;
                if (parse_position(event->data, event->data_len, &lat, &lon)) {
                    if (xSemaphoreTake(gBoatMutex, pdMS_TO_TICKS(5)) == pdTRUE) {
                        gBoat.lat = lat;
                        gBoat.lon = lon;
                        gBoat.gps_valid = true;
                        gBoat.last_update_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;
                        xSemaphoreGive(gBoatMutex);
                        ESP_LOGD(TAG, "Position: %.6f, %.6f", lat, lon);
                    }
                }
            }
            else if (strcmp(topic, MQTT_DEPTH) == 0) {
                float depth = parse_value(event->data, event->data_len);
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
            
            // Wind
            else if (strcmp(topic, MQTT_AWS) == 0) {
                ESP_LOGI( TAG, "MQTT_AWS");
                float aws = parse_value(event->data, event->data_len);
                if (xSemaphoreTake(gBoatMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                    gBoat.aws_ms = aws;
                    if (isnan(gBoat.aws_max_ms) || aws > gBoat.aws_max_ms)
                        gBoat.aws_max_ms = aws;
                    gBoat.last_update_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;
                    xSemaphoreGive(gBoatMutex);
                }
            }
            else if (strcmp(topic, MQTT_AWA) == 0)
                boatSet(gBoat.awa_deg, rad2deg(parse_value(event->data, event->data_len)));
            else if (strcmp(topic, MQTT_TWS) == 0)
                boatSet(gBoat.tws_ms, parse_value(event->data, event->data_len));
            else if (strcmp(topic, MQTT_TWA) == 0)
                boatSet(gBoat.twa_deg, rad2deg(parse_value(event->data, event->data_len)));
            else if (strcmp(topic, MQTT_TWD) == 0)
                boatSet(gBoat.twd_deg, rad2deg(parse_value(event->data, event->data_len)));
            
            // Environment
            else if (strcmp(topic, MQTT_WATER_TEMP) == 0)
                boatSet(gBoat.water_temp_c, K2C(parse_value(event->data, event->data_len)));
            else if (strcmp(topic, MQTT_AIR_TEMP) == 0)
                boatSet(gBoat.air_temp_c, K2C(parse_value(event->data, event->data_len)));
            else if (strcmp(topic, MQTT_PRESSURE) == 0)
                boatSet(gBoat.pressure_hpa, parse_value(event->data, event->data_len) / 100.0f);
            
            // Battery data
            else if (strcmp(topic, MQTT_BATT_HOUSE) == 0) {
                BatteryInfo batt = parse_battery( event->data, event->data_len );
                boatSet(gBoat.house_v, batt.voltage_v);
                boatSet(gBoat.house_a, batt.current_a);
                boatSet(gBoat.house_soc, batt.soc_pct);
            }
            else if (strcmp(topic, MQTT_BATT_HOUSE_LI) == 0) {
                BatteryInfo batt = parse_battery( event->data, event->data_len );
                boatSet(gBoat.house_v_li, batt.voltage_v);
                boatSet(gBoat.house_a_li, batt.current_a);
                boatSet(gBoat.house_li_soc, batt.soc_pct);
            }
            else if (strcmp(topic, MQTT_BATT_START) == 0) {
                BatteryInfo batt = parse_battery( event->data, event->data_len );
                boatSet(gBoat.start_batt_v, batt.voltage_v);
                boatSet(gBoat.start_batt_a, batt.current_a);
                boatSet(gBoat.start_batt_soc, batt.soc_pct);
            }
            else if (strcmp(topic, MQTT_BATT_FWD) == 0) {
                BatteryInfo batt = parse_battery( event->data, event->data_len );
                boatSet(gBoat.forward_v, batt.voltage_v);
                boatSet(gBoat.forward_soc, batt.current_a);
                boatSet(gBoat.forward_soc, batt.soc_pct);
            }
            break;
        }
        
        case MQTT_EVENT_ERROR:
            ESP_LOGW(TAG, "MQTT error");
            break;
            
        default:
            break;
    }
}

// ────────────────────────────────────────────────
//  Public API
// ────────────────────────────────────────────────
void mqtt_start(void) {
    // Use settings for broker address, fall back to defaults
    const char* host = gSettings.mqtt_host[0] ? gSettings.mqtt_host : "192.168.1.100";
    uint16_t port = gSettings.mqtt_port ? gSettings.mqtt_port : 1883;
    
    char uri[128];
    snprintf(uri, sizeof(uri), "mqtt://%s:%d", host, port);
    
    esp_mqtt_client_config_t cfg = {};
    cfg.broker.address.uri = uri;
    cfg.network.reconnect_timeout_ms = 3000;
    cfg.network.timeout_ms = 5000;
    cfg.buffer.size = 4096;
    
    s_client = esp_mqtt_client_init(&cfg);
    esp_mqtt_client_register_event(s_client, MQTT_EVENT_ANY, mqtt_event_handler, NULL);
    esp_mqtt_client_start(s_client);
    
    ESP_LOGI(TAG, "Connecting to %s", uri);
}

void mqtt_stop(void) {
    if (s_client) {
        esp_mqtt_client_stop(s_client);
        esp_mqtt_client_destroy(s_client);
        s_client = NULL;
    }
    if (xSemaphoreTake(gBoatMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
        gBoat.mqtt_connected = false;
        xSemaphoreGive(gBoatMutex);
    }
}
