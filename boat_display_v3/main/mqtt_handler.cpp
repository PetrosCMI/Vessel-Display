#include "mqtt_client.h"
#include "boat_data.h"
#include "esp_log.h"

static const char *TAG = "MQTT_HANDLER";

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    auto event = (esp_mqtt_event_handle_t)event_data;
    switch ((esp_mqtt_event_id_t)event_id) {
        case MQTT_EVENT_CONNECTED:
            esp_mqtt_client_subscribe(event->client, MQTT_TOPIC_DEPTH, 0);
            esp_mqtt_client_subscribe(event->client, MQTT_TOPIC_WIND, 0);
            ESP_LOGI(TAG, "Connected & Subscribed");
            break;
        case MQTT_EVENT_DATA:
            // Parse data and update boat_data mutex-protected variables
            if (strncmp(event->topic, MQTT_TOPIC_DEPTH, event->topic_len) == 0) {
                float depth = atof(event->data);
                boat_data_set_depth(depth); // Uses your existing boat_data.h helpers
            }
            break;
        default:
            break;
    }
}

void start_mqtt() {
    esp_mqtt_client_config_t mqtt_cfg = {};
    mqtt_cfg.broker.address.uri = MQTT_BROKER_URL;
    
    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(client, MQTT_EVENT_ANY, mqtt_event_handler, NULL);
    esp_mqtt_client_start(client);
}
