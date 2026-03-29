#include "ui.h"
#include "page_registry.h"
#include "boat_data.h"
#include "alarm.h"
#include "display_timeout.h"
#include "numpad.h"
#include "alarm_overlay.h"
#include "bsp/esp32_s3_touch_lcd_4b.h"
#include "lvgl.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <vector>
#include <string.h>
#include <stdlib.h>

static const char* TAG = "UI";

// Tabview reference ΓÇö kept for navigation
static lv_obj_t* s_tabview = NULL;

// -- Shared styles ---------------------------------------------
lv_style_t g_style_screen;
lv_style_t g_style_card;
lv_style_t g_style_val_large;
lv_style_t g_style_val_medium;
lv_style_t g_style_label;
lv_style_t g_style_unit;

static void init_styles(void) {
    lv_style_init(&g_style_screen);
    lv_style_set_bg_color(&g_style_screen, C_BG);
    lv_style_set_bg_opa(&g_style_screen, LV_OPA_COVER);

    lv_style_init(&g_style_card);
    lv_style_set_bg_color(&g_style_card, C_CARD);
    lv_style_set_bg_opa(&g_style_card, LV_OPA_COVER);
    lv_style_set_border_color(&g_style_card, C_BORDER);
    lv_style_set_border_width(&g_style_card, 1);
    lv_style_set_radius(&g_style_card, 12);
    lv_style_set_pad_all(&g_style_card, 10);

    lv_style_init(&g_style_val_large);
    lv_style_set_text_font(&g_style_val_large, &lv_font_montserrat_48);
    lv_style_set_text_color(&g_style_val_large, C_TEXT_PRI);

    lv_style_init(&g_style_val_medium);
    lv_style_set_text_font(&g_style_val_medium, &lv_font_montserrat_32);
    lv_style_set_text_color(&g_style_val_medium, C_TEXT_PRI);

    lv_style_init(&g_style_label);
    lv_style_set_text_font(&g_style_label, &lv_font_montserrat_16);
    lv_style_set_text_color(&g_style_label, C_TEXT_SEC);

    lv_style_init(&g_style_unit);
    lv_style_set_text_font(&g_style_unit, &lv_font_montserrat_16);
    lv_style_set_text_color(&g_style_unit, C_ACCENT);
}

// -- Status bar ------------------------------------------------
static lv_obj_t* sb_wifi_icon;
static lv_obj_t* sb_conn_lbl;
static lv_obj_t* sb_age_lbl;
static lv_obj_t* sb_alarm_icon;

static void build_status_bar(lv_obj_t* parent) {
    lv_obj_t* bar = lv_obj_create(parent);
    lv_obj_set_size(bar, 480, 28);
    lv_obj_set_pos(bar, 0, 0);
    lv_obj_set_style_bg_color(bar, lv_color_hex(0x060A12), 0);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(bar, 0, 0);
    lv_obj_set_scrollbar_mode(bar, LV_SCROLLBAR_MODE_OFF);

    sb_wifi_icon = lv_label_create(bar);
    lv_label_set_text(sb_wifi_icon, LV_SYMBOL_WIFI);
    lv_obj_set_style_text_color(sb_wifi_icon, C_RED, 0);
    lv_obj_align(sb_wifi_icon, LV_ALIGN_LEFT_MID, 4, 0);

    sb_conn_lbl = lv_label_create(bar);
    lv_label_set_text(sb_conn_lbl, "Disconnected");
    lv_obj_set_style_text_color(sb_conn_lbl, C_TEXT_SEC, 0);
    lv_obj_set_style_text_font(sb_conn_lbl, &lv_font_montserrat_12, 0);
    lv_obj_align(sb_conn_lbl, LV_ALIGN_LEFT_MID, 28, 0);

    sb_age_lbl = lv_label_create(bar);
    lv_label_set_text(sb_age_lbl, "");
    lv_obj_set_style_text_color(sb_age_lbl, C_TEXT_SEC, 0);
    lv_obj_set_style_text_font(sb_age_lbl, &lv_font_montserrat_12, 0);
    lv_obj_align(sb_age_lbl, LV_ALIGN_CENTER, 0, 0);

    // Alarm bell ΓÇö tappable button, easy to hit at sea (44px wide)
    lv_obj_t* bell_btn = lv_btn_create(bar);
    lv_obj_set_size(bell_btn, 44, 28);
    lv_obj_align(bell_btn, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_set_style_bg_opa(bell_btn, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(bell_btn, 0, 0);
    lv_obj_set_style_shadow_width(bell_btn, 0, 0);
    lv_obj_set_style_pad_all(bell_btn, 0, 0);
    lv_obj_add_event_cb(bell_btn, [](lv_event_t*) {
        if (alarm_any_unacked()) {
            alarm_acknowledge();
        }
    }, LV_EVENT_CLICKED, NULL);

    sb_alarm_icon = lv_label_create(bell_btn);
    lv_label_set_text(sb_alarm_icon, "");
    lv_obj_set_style_text_color(sb_alarm_icon, C_RED, 0);
    lv_obj_center(sb_alarm_icon);
}

static void update_status_bar(void) {
    BoatData d = boatDataSnapshot();

    lv_obj_set_style_text_color(sb_wifi_icon,
        d.signalk_connected ? C_GREEN : C_RED, 0);
    lv_label_set_text(sb_conn_lbl,
        d.signalk_connected ? "SignalK" : "Disconnected");

    if (d.signalk_connected && d.last_update_ms > 0) {
        char buf[20];
        uint32_t age = (xTaskGetTickCount() * portTICK_PERIOD_MS
                        - d.last_update_ms) / 1000;
        if (age < 60) snprintf(buf, sizeof(buf), "%us ago", (unsigned)age);
        else          snprintf(buf, sizeof(buf), ">1min");
        lv_label_set_text(sb_age_lbl, buf);
    } else {
        lv_label_set_text(sb_age_lbl, "");
    }

    if (alarm_any_unacked()) {
        // Flash bell rapidly when unacknowledged
        uint32_t ms = xTaskGetTickCount() * portTICK_PERIOD_MS;
        lv_label_set_text(sb_alarm_icon, (ms / 250) % 2 ? LV_SYMBOL_BELL : "");
        lv_obj_set_style_text_color(sb_alarm_icon, C_RED, 0);
    } else if (alarm_any_active()) {
        // Steady bell when active but acknowledged
        lv_label_set_text(sb_alarm_icon, LV_SYMBOL_BELL);
        lv_obj_set_style_text_color(sb_alarm_icon, C_YELLOW, 0);
    } else {
        lv_label_set_text(sb_alarm_icon, "");
    }
}

// -- Public API ------------------------------------------------
void ui_init(void) {
    // Must be called with BSP display lock held
    init_styles();

    lv_obj_t* scr = lv_screen_active(); // LVGL 9: lv_scr_act() ΓåÆ lv_screen_active()
    lv_obj_add_style(scr, &g_style_screen, 0);
    lv_obj_set_scrollbar_mode(scr, LV_SCROLLBAR_MODE_OFF);

    build_status_bar(scr);
    numpad_init();

    // Tabview ΓÇö bottom tab bar, 46px tall
    lv_obj_t* tv = lv_tabview_create(scr); // LVGL 9: no direction/size args in constructor
    lv_tabview_set_tab_bar_position(tv, LV_DIR_BOTTOM);
    lv_tabview_set_tab_bar_size(tv, 46);
    lv_obj_set_pos(tv, 0, 28);
    lv_obj_set_size(tv, 480, 452);
    lv_obj_set_style_bg_color(tv, C_BG, 0);

    // Style the tab buttons
    lv_obj_t* tab_btns = lv_tabview_get_tab_bar(tv);
    lv_obj_set_style_bg_color(tab_btns, lv_color_hex(0x060A12), 0);
    lv_obj_set_style_text_color(tab_btns, C_TEXT_SEC, 0);

    // Cast to uint32_t to avoid deprecated enum-enum bitwise warning in LVGL 9
    uint32_t items_checked = (uint32_t)LV_PART_ITEMS | (uint32_t)LV_STATE_CHECKED;
    lv_obj_set_style_text_color(tab_btns,   C_ACCENT,             items_checked);
    lv_obj_set_style_border_side(tab_btns,  LV_BORDER_SIDE_TOP,   items_checked);
    lv_obj_set_style_border_color(tab_btns, C_ACCENT,             items_checked);
    lv_obj_set_style_border_width(tab_btns, 2,                    items_checked);

    // Store tabview reference for navigation
    s_tabview = tv;

    // Reset display timeout on any touch
    lv_indev_t* indev = lv_indev_get_next(NULL);
    while (indev) {
        if (lv_indev_get_type(indev) == LV_INDEV_TYPE_POINTER) {
            lv_indev_add_event_cb(indev, [](lv_event_t*) {
                display_timeout_reset();
            }, LV_EVENT_PRESSED, NULL);
        }
        indev = lv_indev_get_next(indev);
    }

    for (const Page& p : ui_get_pages()) {
        // Yield briefly before tab creation ΓÇö lets LVGL process pending redraws
        bsp_display_unlock();
        vTaskDelay(pdMS_TO_TICKS(20));
        bsp_display_lock(0);

        lv_obj_t* tab = lv_tabview_add_tab(tv, p.tab_label);
        lv_obj_set_style_bg_color(tab, C_BG, 0);
        lv_obj_set_style_pad_all(tab, 6, 0);
        lv_obj_set_scrollbar_mode(tab, LV_SCROLLBAR_MODE_OFF);

        p.build_fn(tab);

        // Yield after build
        bsp_display_unlock();
        vTaskDelay(pdMS_TO_TICKS(20));
        bsp_display_lock(0);
    }

    // Build alarm overlay LAST ΓÇö after all pages so it's on top in z-order
    // (lv_layer_top() makes this redundant but belt-and-suspenders)
    alarm_overlay_init();
}

// -- Navigation ------------------------------------------------
struct NavParams { char label[32]; };

static void navigate_async(void* arg) {
    NavParams* p = (NavParams*)arg;
    if (!s_tabview) { free(p); return; }

    const std::vector<Page>& pages = ui_get_pages();
    for (uint32_t i = 0; i < pages.size(); i++) {
        if (strstr(pages[i].tab_label, p->label)) {
            lv_tabview_set_active(s_tabview, i, LV_ANIM_ON);
            ESP_LOGI(TAG, "Navigated to tab %lu: %s", i, p->label);
            break;
        }
    }
    free(p);
}

void ui_navigate_to_page(const char* page_label) {
    NavParams* p = (NavParams*)malloc(sizeof(NavParams));
    if (!p) return;
    strncpy(p->label, page_label, sizeof(p->label) - 1);
    p->label[sizeof(p->label) - 1] = '\0';
    lv_async_call(navigate_async, p);
}

void ui_alarm_wake(const char* page_label) {
    display_wake();
    ui_navigate_to_page(page_label);
}

void ui_update(void) {
    static uint32_t last_update_ms = 0;

    update_status_bar();
    alarm_overlay_update();

    BoatData d = boatDataSnapshot();
    
    // Always update NET page (index 0) regardless of SignalK data
    const std::vector<Page>& pages = ui_get_pages();
    if (!pages.empty()) {
        pages[6].update_fn();  // NET page is first
    }
    
    // Update other pages only when data changes
    if (d.last_update_ms != last_update_ms) {
        last_update_ms = d.last_update_ms;
        for (size_t i = 1; i < pages.size(); i++) {
            pages[i].update_fn();
        }
    }
}
