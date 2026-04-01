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
#include "esp_heap_caps.h"

#include "bsp/esp32_s3_touch_lcd_4b.h"

#include "config.h"
#include "boat_data.h"
#include "settings.h"
#include "signalk.h"
#include "alarm.h"
#include "wifi_config.h"
#include "ui.h"
#include "display_timeout.h"

#if 0   // This stuff is for CPU load monitoring
#define configGENERATE_RUN_TIME_STATS           1
#define configUSE_STATS_FORMATTING_FUNCTIONS    1

extern void vConfigureTimerForRunTimeStats(void);
extern uint32_t ulGetRunTimeCounterValue(void);

#define portCONFIGURE_TIMER_FOR_RUN_TIME_STATS() vConfigureTimerForRunTimeStats()
#define portGET_RUN_TIME_COUNTER_VALUE()         ulGetRunTimeCounterValue()
#endif

// Forward declaration
void page_network_check_autostart(void);

static const char* TAG = "Main";

// ---------------------------------------------
//  WiFi
// ---------------------------------------------
static EventGroupHandle_t s_wifi_eg;
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1
static int s_retry = 0;
#define MAX_RETRY 2

#ifndef CONFIG_BSP_DISPLAY_LVGL_BUF_HEIGHT
#define CONFIG_BSP_DISPLAY_LVGL_BUF_HEIGHT 50           // Must match what is in sdkconfig.defaults
#endif

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

static bool connect_to_ap(const char* ssid, const char* pass);

static bool wifi_connect_with_settings(void) {
    bool found_any_config = false;

    // 1. Loop through all possible slots in gSettings
    for (int i = 0; i < WIFI_MAX_NETWORKS; i++) {
        ESP_LOGI(TAG, "Evaluating connection to slot %d: %s", i, gSettings.wifi[i].ssid);
        // Skip disabled or empty slots
        if (!gSettings.wifi[i].enabled || gSettings.wifi[i].ssid[0] == '\0') {
            continue;
        }

        // --- CRITICAL FIX START ---
        // 1. Stop any ongoing background connection attempts
        esp_wifi_disconnect();
        
        // 2. Small delay to let the Wi-Fi stack state machine reset
        vTaskDelay(pdMS_TO_TICKS(200));

        // 3. Clear the bits so we don't read a "Fail" from a previous loop
        xEventGroupClearBits(s_wifi_eg, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT);

        found_any_config = true;
        const char* current_ssid = gSettings.wifi[i].ssid;
        const char* current_pass = gSettings.wifi[i].pass;

        ESP_LOGI(TAG, "Attempting connection to slot %d: %s ", i, current_ssid);

        // Configure the station
        wifi_config_t wifi_cfg = {};
        wifi_cfg.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
        wifi_cfg.sta.scan_method = WIFI_ALL_CHANNEL_SCAN;
        wifi_cfg.sta.sort_method = WIFI_CONNECT_AP_BY_SIGNAL;

        strncpy((char*)wifi_cfg.sta.ssid, current_ssid, sizeof(wifi_cfg.sta.ssid) - 1);
        strncpy((char*)wifi_cfg.sta.password, current_pass, sizeof(wifi_cfg.sta.password) - 1);

        // Apply config and attempt connect
        esp_wifi_disconnect(); // Ensure we are starting fresh
        esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg);
        
        xEventGroupClearBits(s_wifi_eg, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT);
        s_retry = 0;
        esp_wifi_connect();

        // Wait for result (10-15 seconds is usually enough)
        EventBits_t bits = xEventGroupWaitBits(s_wifi_eg,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE, pdFALSE, pdMS_TO_TICKS(15000));

        if (bits & WIFI_CONNECTED_BIT) {
            ESP_LOGI(TAG, "Successfully connected to %s", current_ssid);
            return true;
        } else {
            ESP_LOGW(TAG, "Failed to connect to %s, trying next slot...", current_ssid);
            // Optional: esp_wifi_stop() / esp_wifi_start() here if the stack hangs
        }
    }

    // 2. Fallback to compile-time WIFI_SSID if no settings worked
    if (strlen(WIFI_SSID) > 0) {
        ESP_LOGI(TAG, "Trying compile-time fallback: %s", WIFI_SSID);
        // You could wrap the logic above into a helper function to avoid repeating it here
        if (connect_to_ap(WIFI_SSID, WIFI_PASSWORD)) {
            return true;
        }
    }

    if (!found_any_config) {
        ESP_LOGW(TAG, "No Wi-Fi networks were enabled in settings.");
    }

    ESP_LOGE(TAG, "All Wi-Fi connection attempts failed.");
    return false;
}

// Helper function to handle the actual connection logic
static bool connect_to_ap(const char* ssid, const char* pass) {
    wifi_config_t wifi_cfg = {}; // Initialize to zeros
    
    // Set nested members manually to avoid C++ designator issues
    wifi_cfg.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    
    // Use strncpy carefully
    strncpy((char*)wifi_cfg.sta.ssid, ssid, sizeof(wifi_cfg.sta.ssid) - 1);
    strncpy((char*)wifi_cfg.sta.password, pass, sizeof(wifi_cfg.sta.password) - 1);

    ESP_LOGI(TAG, "Connecting to %s...", (char*)wifi_cfg.sta.ssid);
    
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg));
    
    xEventGroupClearBits(s_wifi_eg, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT);
    s_retry = 0;
    esp_wifi_connect();

    EventBits_t bits = xEventGroupWaitBits(s_wifi_eg,
        WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
        pdFALSE, pdFALSE, pdMS_TO_TICKS(15000));

    return (bits & WIFI_CONNECTED_BIT) != 0;
}

static bool wifi_init(void) {
    s_wifi_eg = xEventGroupCreate();
    esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();
    esp_netif_create_default_wifi_ap(); 

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);

    esp_event_handler_instance_t h1, h2;
    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                        wifi_event_handler, NULL, &h1);
    esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                        wifi_event_handler, NULL, &h2);

    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_start();
    return wifi_connect_with_settings();
}

// ---------------------------------------------
//  Network reconnect
// ---------------------------------------------
void network_reconnect(void) {
    ESP_LOGI(TAG, "Network reconnect requested");
    signalk_stop();
    esp_wifi_disconnect();
    vTaskDelay(pdMS_TO_TICKS(500));
    if (wifi_connect_with_settings()) {
        vTaskDelay(pdMS_TO_TICKS(500));
        signalk_start();
    }
}

static void lvgl_update_timer_cb(lv_timer_t*) {
    ui_update();
}

static void log_runtime_stats_safe() {
    TaskStatus_t *pxTaskStatusArray;
    volatile UBaseType_t uxArraySize;
    uint32_t ulTotalRunTime;

    // 1. Get the number of tasks to know how much memory to allocate
    uxArraySize = uxTaskGetNumberOfTasks();
    pxTaskStatusArray = (TaskStatus_t *)malloc(uxArraySize * sizeof(TaskStatus_t));

    if (pxTaskStatusArray != NULL) {
        // 2. Populate the array with raw task data
        uxArraySize = uxTaskGetSystemState(pxTaskStatusArray, uxArraySize, &ulTotalRunTime);

        printf("\n--- Modern Task Stats ---\n");
        
        // 3. Prevent divide-by-zero, and account for dual-core (200% -> 100%)
        #ifdef CONFIG_FREERTOS_UNICORE
            ulTotalRunTime /= 100UL; 
        #else
            // Multiply by 2 cores to normalize the total load to 100%
            //ulTotalRunTime = (ulTotalRunTime * 2) / 100UL; 
            // I don't want it normalized over the 2 cores
            ulTotalRunTime /= 100UL; 
        #endif        

        if (ulTotalRunTime > 0) {
            printf("%-16s | %-12s | %s\n", "Task Name", "Abs Time", "% Time");
            printf("-----------------|--------------|--------\n");
            
            for (UBaseType_t x = 0; x < uxArraySize; x++) {
                uint32_t ulStatsAsPercentage = pxTaskStatusArray[x].ulRunTimeCounter / ulTotalRunTime;
                
                if (ulStatsAsPercentage > 0) {
                    printf("%-16s | %-12lu | %lu%%\n",
                           pxTaskStatusArray[x].pcTaskName,
                           pxTaskStatusArray[x].ulRunTimeCounter,
                           ulStatsAsPercentage);
                } else {
                    // Catch tasks that take less than 1% of CPU
                    printf("%-16s | %-12lu | <1%%\n",
                           pxTaskStatusArray[x].pcTaskName,
                           pxTaskStatusArray[x].ulRunTimeCounter);
                }
            }
        } else {
            printf("Total runtime is zero! The ESP_TIMER clock source is NOT running.\n");
        }

        // Free the array
        free(pxTaskStatusArray);
    } else {
        printf("Failed to allocate memory for task status array.\n");
    }
}

// ─────────────────────────────────────────────
//  Heap monitor — logs free heap every 30s
// ─────────────────────────────────────────────
static void heap_monitor_task(void*) {
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(30000));
        ESP_LOGI("HeapMon", "Free internal: %lu  PSRAM: %lu  Min internal ever: %lu",
                 (unsigned long)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
                 (unsigned long)heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
                 (unsigned long)heap_caps_get_minimum_free_size(MALLOC_CAP_INTERNAL));
        // LVGL memory stats
        lv_mem_monitor_t mon;
        lv_mem_monitor(&mon);
        ESP_LOGI("HeapMon", "LVGL Mem Total: %u  Free: %u  Used: %u  Frag: %u%%",
                 (unsigned)mon.total_size,
                 (unsigned)mon.free_size,
                 (unsigned)mon.used_cnt,
                 (unsigned)mon.frag_pct);
        
        log_runtime_stats_safe();
    }
}

// ─────────────────────────────────────────────
//  Alarm tick task
// ─────────────────────────────────────────────
static void alarm_tick_task(void*) {
    for (;;) { alarm_tick(); vTaskDelay(pdMS_TO_TICKS(1000)); }
}

// ---------------------------------------------
//  app_main
// ---------------------------------------------
extern "C" void app_main(void) {
    ESP_LOGI(TAG, "Boat Display starting...");

    gBoatMutex = xSemaphoreCreateMutex();
    settings_init();

    ESP_ERROR_CHECK(bsp_i2c_init());

    bsp_display_cfg_t disp_cfg = {};
    disp_cfg.lvgl_port_cfg            = ESP_LVGL_PORT_INIT_CONFIG();
    disp_cfg.lvgl_port_cfg.task_stack = 16384;   // 16KB — default is ~4KB, not enough for SET page
    disp_cfg.buffer_size              = BSP_LCD_H_RES * CONFIG_BSP_DISPLAY_LVGL_BUF_HEIGHT;
    disp_cfg.double_buffer            = BSP_LCD_DRAW_BUFF_DOUBLE;
    disp_cfg.flags.buff_spiram        = true;
    lv_display_t* disp = bsp_display_start_with_config(&disp_cfg);

    if (!disp) { ESP_LOGE(TAG, "Display init failed"); return; }
    bsp_display_brightness_set(gSettings.brightness_pct);

    esp_codec_dev_handle_t spk = NULL;
    if (bsp_audio_init(NULL) == ESP_OK) {
        spk = bsp_audio_codec_speaker_init();
        if (!spk) ESP_LOGW(TAG, "Speaker init failed");
    }
    alarm_init(spk);

    // Build UI ΓÇö no lv_timer_create, ui_update runs in its own task
    bsp_display_lock(0);
    ui_init();
    lv_timer_create(lvgl_update_timer_cb, 250, NULL);  // ← add this
    bsp_display_unlock();
    display_timeout_init();

    wifi_init();
    page_network_check_autostart();
    signalk_start();

    xTaskCreate(alarm_tick_task, "alarm_tick", 4096, NULL, 3, NULL);
    alarm_beep(AlarmPattern::BEEP_DOUBLE);
    xTaskCreate(heap_monitor_task, "heap_mon", 4096, NULL, 1, NULL);

    ESP_LOGI(TAG, "Boot complete");
}
