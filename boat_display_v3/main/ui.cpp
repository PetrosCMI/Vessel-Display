#include "ui.h"
#include "page_registry.h"
#include "boat_data.h"
#include "alarm.h"
#include "bsp/esp32_s3_touch_lcd_4b.h"
#include "lvgl.h"

// ── Shared styles ─────────────────────────────────────────────
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

// ── Status bar ────────────────────────────────────────────────
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

    sb_alarm_icon = lv_label_create(bar);
    lv_label_set_text(sb_alarm_icon, "");
    lv_obj_set_style_text_color(sb_alarm_icon, C_RED, 0);
    lv_obj_align(sb_alarm_icon, LV_ALIGN_RIGHT_MID, -4, 0);
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

    if (alarm_any_active()) {
        uint32_t ms = xTaskGetTickCount() * portTICK_PERIOD_MS;
        lv_label_set_text(sb_alarm_icon, (ms / 500) % 2 ? LV_SYMBOL_BELL : "");
    } else {
        lv_label_set_text(sb_alarm_icon, "");
    }
}

// ── Public API ────────────────────────────────────────────────
void ui_init(void) {
    // Must be called with BSP display lock held
    init_styles();

    lv_obj_t* scr = lv_screen_active(); // LVGL 9: lv_scr_act() → lv_screen_active()
    lv_obj_add_style(scr, &g_style_screen, 0);
    lv_obj_set_scrollbar_mode(scr, LV_SCROLLBAR_MODE_OFF);

    build_status_bar(scr);

    // Tabview — bottom tab bar, 46px tall
    lv_obj_t* tv = lv_tabview_create(scr); // LVGL 9: no direction/size args in constructor
    lv_tabview_set_tab_bar_position(tv, LV_DIR_BOTTOM);
    lv_tabview_set_tab_bar_size(tv, 46);
    lv_obj_set_pos(tv, 0, 28);
    lv_obj_set_size(tv, 480, 452);
    lv_obj_set_style_bg_color(tv, C_BG, 0);

    // Style the tab buttons
    lv_obj_t* tab_btns = lv_tabview_get_tab_bar(tv); // LVGL 9: get_tab_btns → get_tab_bar
    lv_obj_set_style_bg_color(tab_btns, lv_color_hex(0x060A12), 0);
    lv_obj_set_style_text_color(tab_btns, C_TEXT_SEC, 0);
    lv_obj_set_style_text_color(tab_btns, C_ACCENT,
        LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_border_side(tab_btns, LV_BORDER_SIDE_TOP,
        LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_border_color(tab_btns, C_ACCENT,
        LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_border_width(tab_btns, 2,
        LV_PART_ITEMS | LV_STATE_CHECKED);

    for (const Page& p : ui_get_pages()) {
        lv_obj_t* tab = lv_tabview_add_tab(tv, p.tab_label);
        lv_obj_set_style_bg_color(tab, C_BG, 0);
        lv_obj_set_style_pad_all(tab, 6, 0);
        lv_obj_set_scrollbar_mode(tab, LV_SCROLLBAR_MODE_OFF);
        p.build_fn(tab);
    }
}

void ui_update(void) {
    // Called from a timer task — BSP lock is already held by the LVGL port task
    // when this is triggered via lv_timer_create(). If called from app_main loop,
    // wrap in bsp_display_lock()/unlock().
    update_status_bar();
    for (const Page& p : ui_get_pages()) {
        p.update_fn();
    }
}
