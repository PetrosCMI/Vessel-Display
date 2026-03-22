// page_network.cpp — Network status display + web config trigger
#include "page_registry.h"
#include "ui.h"
#include "settings.h"
#include "wifi_config.h"
#include "boat_data.h"
#include "alarm.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"
#include <string.h>
#include <stdio.h>

#define EVT_OBJ(e) ((lv_obj_t*)lv_event_get_target(e))

// ── Background tasks for WiFi operations ─────────────────────
// WiFi mode changes must never run in the LVGL task
static void start_config_task(void*) {
    wifi_config_start();
    vTaskDelete(NULL);
}

static void stop_config_task(void*) {
    wifi_config_stop();
    vTaskDelay(pdMS_TO_TICKS(500));
    esp_restart();   // cleanest approach — fresh WiFi stack on reboot
    vTaskDelete(NULL);
}

// ── Widgets ───────────────────────────────────────────────────
static lv_obj_t* s_wifi_status_lbl  = NULL;
static lv_obj_t* s_ip_lbl           = NULL;
static lv_obj_t* s_sk_status_lbl    = NULL;
static lv_obj_t* s_sk_host_lbl      = NULL;
static lv_obj_t* s_config_card      = NULL;
static lv_obj_t* s_config_instr_lbl = NULL;
static lv_obj_t* s_btn_config       = NULL;
static lv_obj_t* s_btn_stop         = NULL;
static lv_obj_t* s_uptime_lbl       = NULL;

// ── Build ─────────────────────────────────────────────────────
static void build(lv_obj_t* tab) {
    int y = 0;

    // ── WiFi status card ─────────────────────────────────────
    lv_obj_t* wcard = lv_obj_create(tab);
    lv_obj_add_style(wcard, &g_style_card, 0);
    lv_obj_set_size(wcard, 454, 100);
    lv_obj_set_pos(wcard, 0, y);
    lv_obj_set_scrollbar_mode(wcard, LV_SCROLLBAR_MODE_OFF);

    lv_obj_t* wt = lv_label_create(wcard);
    lv_label_set_text(wt, "WIFI");
    lv_obj_add_style(wt, &g_style_label, 0);
    lv_obj_set_pos(wt, 0, 0);

    s_wifi_status_lbl = lv_label_create(wcard);
    lv_label_set_text(s_wifi_status_lbl, "Connecting...");
    lv_obj_set_style_text_font(s_wifi_status_lbl, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(s_wifi_status_lbl, C_TEXT_PRI, 0);
    lv_obj_set_pos(s_wifi_status_lbl, 0, 22);

    s_ip_lbl = lv_label_create(wcard);
    lv_label_set_text(s_ip_lbl, "");
    lv_obj_add_style(s_ip_lbl, &g_style_label, 0);
    lv_obj_set_pos(s_ip_lbl, 0, 46);
    y += 110;

    // ── SignalK status card ───────────────────────────────────
    lv_obj_t* scard = lv_obj_create(tab);
    lv_obj_add_style(scard, &g_style_card, 0);
    lv_obj_set_size(scard, 454, 100);
    lv_obj_set_pos(scard, 0, y);
    lv_obj_set_scrollbar_mode(scard, LV_SCROLLBAR_MODE_OFF);

    lv_obj_t* st = lv_label_create(scard);
    lv_label_set_text(st, "SIGNALK");
    lv_obj_add_style(st, &g_style_label, 0);
    lv_obj_set_pos(st, 0, 0);

    s_sk_status_lbl = lv_label_create(scard);
    lv_label_set_text(s_sk_status_lbl, "Disconnected");
    lv_obj_set_style_text_font(s_sk_status_lbl, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(s_sk_status_lbl, C_RED, 0);
    lv_obj_set_pos(s_sk_status_lbl, 0, 22);

    s_sk_host_lbl = lv_label_create(scard);
    char skbuf[80];
    snprintf(skbuf, sizeof(skbuf), "%s:%d",
             gSettings.signalk_host, gSettings.signalk_port);
    lv_label_set_text(s_sk_host_lbl, skbuf);
    lv_obj_add_style(s_sk_host_lbl, &g_style_label, 0);
    lv_obj_set_pos(s_sk_host_lbl, 0, 46);

    s_uptime_lbl = lv_label_create(scard);
    lv_label_set_text(s_uptime_lbl, "Uptime: --");
    lv_obj_add_style(s_uptime_lbl, &g_style_label, 0);
    lv_obj_set_pos(s_uptime_lbl, 240, 46);
    y += 110;

    // ── Config card ───────────────────────────────────────────
    s_config_card = lv_obj_create(tab);
    lv_obj_add_style(s_config_card, &g_style_card, 0);
    lv_obj_set_size(s_config_card, 454, 150);
    lv_obj_set_pos(s_config_card, 0, y);
    lv_obj_set_scrollbar_mode(s_config_card, LV_SCROLLBAR_MODE_OFF);

    lv_obj_t* ct = lv_label_create(s_config_card);
    lv_label_set_text(ct, "CONFIGURATION");
    lv_obj_add_style(ct, &g_style_label, 0);
    lv_obj_set_pos(ct, 0, 0);

    s_config_instr_lbl = lv_label_create(s_config_card);
    lv_label_set_text(s_config_instr_lbl,
        "Connect phone to:\n"
        LV_SYMBOL_WIFI "  BoatDisplay\n"
        "Then open:  192.168.4.1");
    lv_obj_set_style_text_color(s_config_instr_lbl, C_ACCENT, 0);
    lv_obj_set_style_text_font(s_config_instr_lbl, &lv_font_montserrat_16, 0);
    lv_obj_set_pos(s_config_instr_lbl, 0, 22);
    lv_obj_add_flag(s_config_instr_lbl, LV_OBJ_FLAG_HIDDEN);

    // Start config button
    s_btn_config = lv_btn_create(s_config_card);
    lv_obj_set_size(s_btn_config, 200, 44);
    lv_obj_set_pos(s_btn_config, 0, 96);
    lv_obj_set_style_bg_color(s_btn_config, C_ACCENT, 0);
    lv_obj_t* bcl = lv_label_create(s_btn_config);
    lv_label_set_text(bcl, LV_SYMBOL_WIFI "  Start Config");
    lv_obj_set_style_text_color(bcl, lv_color_hex(0x000000), 0);
    lv_obj_center(bcl);
    lv_obj_add_event_cb(s_btn_config, [](lv_event_t*) {
        xTaskCreate(start_config_task, "start_cfg", 4096, NULL, 5, NULL);
        alarm_beep(AlarmPattern::BEEP_ONCE);
    }, LV_EVENT_CLICKED, NULL);

    // Stop config button (hidden until config active)
    s_btn_stop = lv_btn_create(s_config_card);
    lv_obj_set_size(s_btn_stop, 200, 44);
    lv_obj_set_pos(s_btn_stop, 0, 96);
    lv_obj_set_style_bg_color(s_btn_stop, lv_color_hex(0x401010), 0);
    lv_obj_t* bsl = lv_label_create(s_btn_stop);
    lv_label_set_text(bsl, LV_SYMBOL_CLOSE "  Stop Config");
    lv_obj_center(bsl);
    lv_obj_add_flag(s_btn_stop, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(s_btn_stop, [](lv_event_t*) {
        xTaskCreate(stop_config_task, "stop_cfg", 4096, NULL, 5, NULL);
    }, LV_EVENT_CLICKED, NULL);

    // Restart button
    lv_obj_t* rst_btn = lv_btn_create(s_config_card);
    lv_obj_set_size(rst_btn, 200, 44);
    lv_obj_set_pos(rst_btn, 210, 96);
    lv_obj_set_style_bg_color(rst_btn, lv_color_hex(0x303040), 0);
    lv_obj_t* rbl = lv_label_create(rst_btn);
    lv_label_set_text(rbl, LV_SYMBOL_REFRESH "  Restart");
    lv_obj_center(rbl);
    lv_obj_add_event_cb(rst_btn, [](lv_event_t*) {
        vTaskDelay(pdMS_TO_TICKS(300));
        esp_restart();
    }, LV_EVENT_CLICKED, NULL);
}

// ── Update — refresh status every 250ms ──────────────────────
static void update(void) {
    if (!s_wifi_status_lbl) return;

    bool config_active = wifi_config_active();

    // Show/hide config UI elements
    if (config_active) {
        lv_obj_clear_flag(s_config_instr_lbl, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_btn_config,          LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(s_btn_stop,          LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(s_config_instr_lbl,    LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(s_btn_config,        LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_btn_stop,            LV_OBJ_FLAG_HIDDEN);
    }

    // WiFi status
    wifi_ap_record_t ap_info;
    if (config_active) {
        lv_label_set_text(s_wifi_status_lbl, "Config AP: BoatDisplay");
        lv_obj_set_style_text_color(s_wifi_status_lbl, C_YELLOW, 0);
        lv_label_set_text(s_ip_lbl, "192.168.4.1");
    } else if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
        char buf[48];
        snprintf(buf, sizeof(buf), "%s  (%ddBm)", ap_info.ssid, ap_info.rssi);
        lv_label_set_text(s_wifi_status_lbl, buf);
        lv_obj_set_style_text_color(s_wifi_status_lbl, C_GREEN, 0);

        // Get IP
        esp_netif_t* netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
        if (netif) {
            esp_netif_ip_info_t ip_info;
            if (esp_netif_get_ip_info(netif, &ip_info) == ESP_OK) {
                char ipbuf[20];
                esp_ip4addr_ntoa(&ip_info.ip, ipbuf, sizeof(ipbuf));
                lv_label_set_text(s_ip_lbl, ipbuf);
            }
        }
    } else {
        lv_label_set_text(s_wifi_status_lbl, "Not connected");
        lv_obj_set_style_text_color(s_wifi_status_lbl, C_RED, 0);
        lv_label_set_text(s_ip_lbl, "");
    }

    // SignalK status
    BoatData d = boatDataSnapshot();
    if (d.signalk_connected) {
        lv_label_set_text(s_sk_status_lbl, "Connected");
        lv_obj_set_style_text_color(s_sk_status_lbl, C_GREEN, 0);
    } else {
        lv_label_set_text(s_sk_status_lbl, "Disconnected");
        lv_obj_set_style_text_color(s_sk_status_lbl, C_RED, 0);
    }

    // Update SignalK host label in case it changed
    char skbuf[80];
    snprintf(skbuf, sizeof(skbuf), "%s:%d",
             gSettings.signalk_host, gSettings.signalk_port);
    lv_label_set_text(s_sk_host_lbl, skbuf);

    // Uptime
    uint32_t secs = xTaskGetTickCount() * portTICK_PERIOD_MS / 1000;
    uint32_t h    = secs / 3600;
    uint32_t m    = (secs % 3600) / 60;
    uint32_t s2   = secs % 60;
    char ubuf[24];
    snprintf(ubuf, sizeof(ubuf), "Up %02lu:%02lu:%02lu",
             (unsigned long)h, (unsigned long)m, (unsigned long)s2);
    lv_label_set_text(s_uptime_lbl, ubuf);
}

// ── Auto-start AP if no WiFi configured ──────────────────────
static struct NetReg {
    NetReg() {
        ui_register_page({ LV_SYMBOL_WIFI " NET", build, update });
    }
} _reg;

// Called from app_main after settings_init()
void page_network_check_autostart(void) {
    bool any = false;
    for (int i = 0; i < WIFI_MAX_NETWORKS; i++) {
        if (gSettings.wifi[i].ssid[0]) { any = true; break; }
    }
    if (!any) {
        ESP_LOGI("Network", "No WiFi configured — starting config AP");
        wifi_config_start();
    }
}