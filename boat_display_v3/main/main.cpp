#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "nvs_flash.h"

#include "bsp/esp32_s3_touch_lcd_4b.h"

#include "config.h"
#include "boat_data.h"
#include "settings.h"
#include "signalk.h"
#include "alarm.h"
#include "ui.h"

// Page self-registrations happen via static constructors.
// Listing the externs here forces the linker to include each TU.
// (If using CMake with explicit SRCS, this is already guaranteed.)
extern void _page_nav_reg();    // not actually called — just forces linkage
extern void _page_wind_reg();
extern void _page_depth_reg();
extern void _page_engine_reg();
extern void _page_settings_reg();

static const char* TAG = "Main";

// ─────────────────────────────────────────────
//  WiFi
// ─────────────────────────────────────────────
static EventGroupHandle_t s_wifi_eg;
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

static int s_retry = 0;
#define MAX_RETRY 10

static void wifi_event_handler(void* arg, esp_event_base_t base,
                                int32_t id, void* data) {
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry < MAX_RETRY) {
            esp_wifi_connect();
            s_retry++;
            ESP_LOGI(TAG, "WiFi retry %d/%d", s_retry, MAX_RETRY);
        } else {
            xEventGroupSetBits(s_wifi_eg, WIFI_FAIL_BIT);
        }
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* ev = (ip_event_got_ip_t*)data;
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&ev->ip_info.ip));
        s_retry = 0;
        xEventGroupSetBits(s_wifi_eg, WIFI_CONNECTED_BIT);
    }
}

static bool wifi_init(void) {
    s_wifi_eg = xEventGroupCreate();

    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    esp_event_handler_instance_t h1, h2;
    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                        wifi_event_handler, NULL, &h1);
    esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                        wifi_event_handler, NULL, &h2);

    wifi_config_t wifi_cfg = {};
    strncpy((char*)wifi_cfg.sta.ssid,     WIFI_SSID,     sizeof(wifi_cfg.sta.ssid) - 1);
    strncpy((char*)wifi_cfg.sta.password, WIFI_PASSWORD, sizeof(wifi_cfg.sta.password) - 1);
    wifi_cfg.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg);
    esp_wifi_start();
    esp_wifi_connect();

    ESP_LOGI(TAG, "Connecting to SSID: %s", WIFI_SSID);

    // Wait up to 15s for connection
    EventBits_t bits = xEventGroupWaitBits(s_wifi_eg,
        WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
        pdFALSE, pdFALSE, pdMS_TO_TICKS(15000));

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "WiFi connected");
        return true;
    }
    ESP_LOGW(TAG, "WiFi connection failed — will retry in background");
    return false;
}

// ─────────────────────────────────────────────
//  LVGL update timer  (called inside LVGL task,
//  so BSP lock is already held)
// ─────────────────────────────────────────────
static void lvgl_update_timer_cb(lv_timer_t* t) {
    ui_update();
}

// ─────────────────────────────────────────────
//  Alarm tick task  (1 Hz, separate task)
// ─────────────────────────────────────────────
static void alarm_tick_task(void* arg) {
    for (;;) {
        alarm_tick();
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

// ─────────────────────────────────────────────
//  app_main
// ─────────────────────────────────────────────
extern "C" void app_main(void) {
    ESP_LOGI(TAG, "Boat Display (ESP-IDF + BSP) starting...");

    // Shared data mutex
    gBoatMutex = xSemaphoreCreateMutex();

    // Settings from NVS (also initialises nvs_flash)
    settings_init();

    // ── BSP: I2C (required before display, touch, audio) ──
    ESP_ERROR_CHECK(bsp_i2c_init());

    // ── BSP: Display + touch + LVGL port task ─────────────
    // bsp_display_start() launches the LVGL task internally.
    // All lv_...() calls after this must be wrapped in
    // bsp_display_lock() / bsp_display_unlock().
    lv_display_t* disp = bsp_display_start();
    if (!disp) {
        ESP_LOGE(TAG, "Display init failed");
        return;
    }
    bsp_display_brightness_set(gSettings.brightness_pct);
    ESP_LOGI(TAG, "Display OK");

    // ── BSP: Audio (ES8311 speaker) ───────────────────────
    esp_codec_dev_handle_t spk = NULL;
    if (bsp_audio_init(NULL) == ESP_OK) {
        spk = bsp_audio_codec_speaker_init();
        if (!spk) ESP_LOGW(TAG, "Speaker codec init failed — audio disabled");
    } else {
        ESP_LOGW(TAG, "Audio init failed — audio disabled");
    }

    // ── Alarm engine ──────────────────────────────────────
    alarm_init(spk);

    // ── Build UI (must be inside BSP lock) ────────────────
    bsp_display_lock(0);
    ui_init();
    // Register a 250 ms LVGL timer for UI value updates.
    // Runs inside the LVGL task — no extra locking needed.
    lv_timer_create(lvgl_update_timer_cb, 250, NULL);
    bsp_display_unlock();

    ESP_LOGI(TAG, "UI built");

    // ── WiFi ──────────────────────────────────────────────
    wifi_init();

    // ── SignalK WebSocket (starts its own task) ───────────
    signalk_start();

    // ── Alarm tick task ───────────────────────────────────
    xTaskCreate(alarm_tick_task, "alarm_tick", 3072, NULL, 3, NULL);

    // ── Startup beep ──────────────────────────────────────
    alarm_beep(AlarmPattern::BEEP_DOUBLE);

    ESP_LOGI(TAG, "Boot complete — running");

    // app_main can return; the LVGL port task, alarm task,
    // and WebSocket task keep everything alive.
}
