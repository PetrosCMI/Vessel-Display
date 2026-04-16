// display_timeout.cpp ΓÇö backlight dim/off after inactivity
#include "display_timeout.h"
#include "settings.h"
#include "bsp/esp32_s3_touch_lcd_4b.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include <stdint.h>

static const char* TAG = "Timeout";

#define DIM_BRIGHTNESS 5   // 5% when dimmed

typedef enum {
    STATE_AWAKE,
    STATE_DIMMED,
    STATE_OFF,
} DisplayState;

static volatile uint32_t  s_last_activity_ms = 0;
static volatile bool      s_force_wake       = false;
static DisplayState        s_state            = STATE_AWAKE;

static uint32_t now_ms(void) {
    return xTaskGetTickCount() * portTICK_PERIOD_MS;
}

void display_timeout_reset(void) {
    s_last_activity_ms = now_ms();
    if (s_state != STATE_AWAKE) {
        // Wake immediately — don't wait for the 5s task cycle
        s_state = STATE_AWAKE;
        bsp_display_brightness_set(gSettings.brightness_pct);
        ESP_LOGD(TAG, "Display wake (touch)");
    }
    s_force_wake = false;  // no need for task to process it
}

void display_wake(void) {
    s_last_activity_ms = now_ms();
    if (s_state != STATE_AWAKE) {
        s_state = STATE_AWAKE;
        bsp_display_brightness_set(gSettings.brightness_pct);
        ESP_LOGD(TAG, "Display wake (alarm)");
    }
    s_force_wake = false;

}

bool display_is_sleeping(void) {
    return s_state != STATE_AWAKE;
}

static void set_brightness(int pct) {
    bsp_display_brightness_set(pct);
}

static void timeout_task(void*) {
    vTaskDelay(pdMS_TO_TICKS(3000));
    s_last_activity_ms = now_ms();

    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(2000));

        // Read timeout from settings ΓÇö allows runtime changes to take effect
        uint32_t dim_ms = (uint32_t)gSettings.display_timeout_minutes * 60 * 1000;
        uint32_t off_ms = dim_ms + 60 * 1000;  // always 1 minute after dim

        ESP_LOGD(TAG, "Display timer(%d) s_force_wake: %d",
                    gSettings.display_timeout_minutes, s_force_wake);
        if (s_force_wake) {
            s_force_wake = false;
            // Wake already handled directly in display_wake/reset
            // This is just a safety catch
            if (s_state != STATE_AWAKE) {
                set_brightness(gSettings.brightness_pct);
                s_state = STATE_AWAKE;
            }
            continue;
        }

        uint32_t idle_ms = now_ms() - s_last_activity_ms;
        ESP_LOGD(TAG, "idle=%lus dim_at=%lus state=%d",
                 (unsigned long)(idle_ms/1000),
                 (unsigned long)(dim_ms/1000),
                 s_state);

        if (s_state == STATE_AWAKE && idle_ms >= dim_ms) {
            ESP_LOGI(TAG, "Display dimming after %d min",
                     gSettings.display_timeout_minutes);
            set_brightness(DIM_BRIGHTNESS);
            s_state = STATE_DIMMED;
        } else if (s_state == STATE_DIMMED && idle_ms >= off_ms) {
            ESP_LOGI(TAG, "Display off");
            set_brightness(0);
            s_state = STATE_OFF;
        }
    }
}

void display_timeout_init(void) {
    xTaskCreate(timeout_task, "disp_timeout", 4096, NULL, 2, NULL);
    ESP_LOGD(TAG, "Display timeout: dim at %d min, off at %d min",
             gSettings.display_timeout_minutes,
             gSettings.display_timeout_minutes + 1);
}
