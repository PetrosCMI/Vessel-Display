#include "ui.h"
#include "page_registry.h"
#include "boat_data.h"
#include "alarm.h"
#include "settings.h"
#include <lvgl.h>

// ═══════════════════════════════════════════════════════════════
//  SHARED STYLES  (extern declared in ui.h, used by page files)
// ═══════════════════════════════════════════════════════════════
lv_style_t g_style_screen;
lv_style_t g_style_card;
lv_style_t g_style_val_large;
lv_style_t g_style_val_medium;
lv_style_t g_style_label;
lv_style_t g_style_unit;

const lv_color_t C_BG       = LV_COLOR_MAKE(0x0A, 0x0E, 0x17);
const lv_color_t C_CARD     = LV_COLOR_MAKE(0x11, 0x18, 0x27);
const lv_color_t C_BORDER   = LV_COLOR_MAKE(0x1E, 0x3A, 0x5F);
const lv_color_t C_ACCENT   = LV_COLOR_MAKE(0x00, 0xBF, 0xFF);
const lv_color_t C_GREEN    = LV_COLOR_MAKE(0x00, 0xE6, 0x76);
const lv_color_t C_YELLOW   = LV_COLOR_MAKE(0xFF, 0xD6, 0x00);
const lv_color_t C_RED      = LV_COLOR_MAKE(0xFF, 0x17, 0x44);
const lv_color_t C_TEXT_PRI = LV_COLOR_MAKE(0xE8, 0xF0, 0xFE);
const lv_color_t C_TEXT_SEC = LV_COLOR_MAKE(0x60, 0x7D, 0x8B);
const lv_color_t C_WIND     = LV_COLOR_MAKE(0x40, 0xC4, 0xFF);
const lv_color_t C_DEPTH    = LV_COLOR_MAKE(0x1D, 0xE9, 0xB6);

static void init_styles() {
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

// ═══════════════════════════════════════════════════════════════
//  STATUS BAR
// ═══════════════════════════════════════════════════════════════
static lv_obj_t* sb_wifi_icon;
static lv_obj_t* sb_conn_lbl;
static lv_obj_t* sb_age_lbl;
static lv_obj_t* sb_alarm_icon;

static void build_status_bar(lv_obj_t* parent) {
    lv_style_t st;
    lv_style_init(&st);
    lv_style_set_bg_color(&st, LV_COLOR_MAKE(0x06, 0x0A, 0x12));
    lv_style_set_bg_opa(&st, LV_OPA_COVER);

    lv_obj_t* bar = lv_obj_create(parent);
    lv_obj_set_size(bar, 480, 28);
    lv_obj_set_pos(bar, 0, 0);
    lv_obj_add_style(bar, &st, 0);
    lv_obj_set_scrollbar_mode(bar, LV_SCROLLBAR_MODE_OFF);

    sb_wifi_icon = lv_label_create(bar);
    lv_label_set_text(sb_wifi_icon, LV_SYMBOL_WIFI);
    lv_obj_set_style_text_color(sb_wifi_icon, C_RED, 0);
    lv_obj_align(sb_wifi_icon, LV_ALIGN_LEFT_MID, 4, 0);

    sb_conn_lbl = lv_label_create(bar);
    lv_label_set_text(sb_conn_lbl, "No Signal");
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

static void update_status_bar() {
    BoatData d = boatDataSnapshot();
    lv_obj_set_style_text_color(sb_wifi_icon,
        d.signalk_connected ? C_GREEN : C_RED, 0);
    lv_label_set_text(sb_conn_lbl,
        d.signalk_connected ? "SignalK" : "Disconnected");
    if (d.signalk_connected && d.last_update_ms > 0) {
        char buf[20];
        unsigned long age = (millis() - d.last_update_ms) / 1000;
        snprintf(buf, sizeof(buf), age < 60 ? "%lus ago" : ">1min", age);
        lv_label_set_text(sb_age_lbl, buf);
    } else {
        lv_label_set_text(sb_age_lbl, "");
    }
    if (alarm_any_active()) {
        lv_label_set_text(sb_alarm_icon,
            ((millis() / 500) % 2) ? LV_SYMBOL_BELL : "");
    } else {
        lv_label_set_text(sb_alarm_icon, "");
    }
}

// ═══════════════════════════════════════════════════════════════
//  PUBLIC API
// ═══════════════════════════════════════════════════════════════
void ui_init() {
    init_styles();

    lv_obj_t* scr = lv_scr_act();
    lv_obj_add_style(scr, &g_style_screen, 0);
    lv_obj_set_scrollbar_mode(scr, LV_SCROLLBAR_MODE_OFF);

    build_status_bar(scr);

    lv_obj_t* tv = lv_tabview_create(scr, LV_DIR_BOTTOM, 46);
    lv_obj_set_pos(tv, 0, 28);
    lv_obj_set_size(tv, 480, 452);
    lv_obj_set_style_bg_color(tv, C_BG, 0);

    lv_obj_t* tab_btns = lv_tabview_get_tab_btns(tv);
    lv_obj_set_style_bg_color(tab_btns, LV_COLOR_MAKE(0x06, 0x0A, 0x12), 0);
    lv_obj_set_style_text_color(tab_btns, C_TEXT_SEC, 0);
    lv_obj_set_style_text_color(tab_btns, C_ACCENT, LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_border_side(tab_btns, LV_BORDER_SIDE_TOP, LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_border_color(tab_btns, C_ACCENT, LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_border_width(tab_btns, 2, LV_PART_ITEMS | LV_STATE_CHECKED);

    for (const Page& p : ui_get_pages()) {
        lv_obj_t* tab = lv_tabview_add_tab(tv, p.tab_label);
        lv_obj_set_style_bg_color(tab, C_BG, 0);
        lv_obj_set_style_pad_all(tab, 6, 0);
        lv_obj_set_scrollbar_mode(tab, LV_SCROLLBAR_MODE_OFF);
        p.build_fn(tab);
    }
}

void ui_update() {
    update_status_bar();
    for (const Page& p : ui_get_pages()) {
        p.update_fn();
    }
}
