// alarm_overlay.cpp ΓÇö full-screen alarm notification overlay
#include "alarm_overlay.h"
#include "alarm.h"
#include "boat_data.h"
#include "settings.h"
#include "units.h"
#include "ui.h"
#include "lvgl.h"
#include "esp_log.h"
#include <stdio.h>
#include <math.h>

static const char* TAG = "AlarmOverlay";

// ΓöÇΓöÇ Widgets ΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇ
static lv_obj_t* s_overlay      = NULL;
static lv_obj_t* s_icon_lbl     = NULL;
static lv_obj_t* s_title_lbl    = NULL;
static lv_obj_t* s_cur_row_lbl  = NULL;
static lv_obj_t* s_cur_val_lbl  = NULL;
static lv_obj_t* s_set_row_lbl  = NULL;
static lv_obj_t* s_set_val_lbl  = NULL;
static lv_obj_t* s_ack_btn_lbl  = NULL;

static AlarmID   s_shown_id     = ALARM_COUNT;
static bool      s_visible      = false;

// ΓöÇΓöÇ Helpers ΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇ
static void fmt_depth_cur(char* buf, size_t n) {
    BoatData d = boatDataSnapshot();
    if (isnan(d.depth_m)) { snprintf(buf, n, "---"); return; }
    if (gSettings.depth_unit == DepthUnit::FEET)
        snprintf(buf, n, "%.1f ft", d.depth_m * 3.28084f);
    else
        snprintf(buf, n, "%.1f m", d.depth_m);
}

static void fmt_wind_cur(char* buf, size_t n) {
    BoatData d = boatDataSnapshot();
    if (isnan(d.aws_ms)) { snprintf(buf, n, "---"); return; }
    char tmp[24];
    const char* unit = fmt_speed(d.aws_ms, tmp, sizeof(tmp));
    snprintf(buf, n, "%s %s", tmp, unit);
}

static void fmt_threshold(char* buf, size_t n, AlarmID id, bool is_hi) {
    float si = is_hi ? gSettings.alarm_hi[id] : gSettings.alarm_lo[id];
    float dv = alarm_threshold_display(id, si);
    if (isnan(dv)) { snprintf(buf, n, "---"); return; }
    const char* unit = alarm_threshold_unit(id);
    bool is_depth = (id == ALARM_DEPTH_MIN || id == ALARM_DEPTH_MAX);
    bool metres   = is_depth && (gSettings.depth_unit == DepthUnit::METERS);
    if (metres) snprintf(buf, n, "%.1f %s", dv, unit);
    else        snprintf(buf, n, "%.0f %s", dv, unit);
}

static bool find_active_alarm(AlarmID* out_id) {
    for (int i = 0; i < ALARM_COUNT; i++) {
        if (alarm_is_active((AlarmID)i) && !alarm_is_acked((AlarmID)i)) {
            *out_id = (AlarmID)i;
            return true;
        }
    }
    return false;
}

static void refresh(AlarmID id) {
    lv_label_set_text(s_title_lbl, ALARM_TABLE[id].label);

    bool is_depth  = (id == ALARM_DEPTH_MIN || id == ALARM_DEPTH_MAX);
    bool is_wind   = (id == ALARM_WIND_SPEED_MAX);
    bool is_batt   = (id == ALARM_BATT_HOUSE ||
                      id == ALARM_BATT_START ||
                      id == ALARM_BATT_FORWARD);
    bool is_anchor = (id == ALARM_ANCHOR_DRAG);
    bool is_storm  = (id == ALARM_STORM);

    // Current value
    lv_label_set_text(s_cur_row_lbl,
        is_depth  ? "Depth:"      :
        is_wind   ? "Wind Speed:" :
        is_batt   ? "Voltage:"    :
        is_anchor ? "Drift:"      :
        is_storm  ? "Storm Level:": "Current:");

    char cur[32];
    if (is_depth) {
        fmt_depth_cur(cur, sizeof(cur));
    } else if (is_wind) {
        fmt_wind_cur(cur, sizeof(cur));
    } else if (is_batt) {
        BoatData d = boatDataSnapshot();
        float v = (id == ALARM_BATT_HOUSE)   ? d.house_v :
                  (id == ALARM_BATT_START)   ? d.start_batt_v :
                                               d.forward_v;
        if (isnan(v)) snprintf(cur, sizeof(cur), "---");
        else          snprintf(cur, sizeof(cur), "%.2f V", v);
    } else if (is_anchor) {
        BoatData d = boatDataSnapshot();
        if (isnan(d.anchor_dist_m)) snprintf(cur, sizeof(cur), "---");
        else snprintf(cur, sizeof(cur), "%.0f m", d.anchor_dist_m);
    } else if (is_storm) {
        BoatData d = boatDataSnapshot();
        if (isnan(d.storm_level)) snprintf(cur, sizeof(cur), "---");
        else snprintf(cur, sizeof(cur), "%.0f", d.storm_level);
    } else {
        snprintf(cur, sizeof(cur), "---");
    }
    lv_label_set_text(s_cur_val_lbl, cur);

    // Setpoint
    char set[32];
    if (id == ALARM_DEPTH_MIN) {
        lv_label_set_text(s_set_row_lbl, "Minimum:");
        fmt_threshold(set, sizeof(set), id, false);
    } else if (is_batt) {
        lv_label_set_text(s_set_row_lbl, "Low limit:");
        snprintf(set, sizeof(set), "%.1f V", gSettings.battery_low_v);
    } else if (is_anchor) {
        lv_label_set_text(s_set_row_lbl, "Radius:");
        snprintf(set, sizeof(set), "%.0f m", gSettings.anchor_radius_m);
    } else if (is_storm) {
        lv_label_set_text(s_set_row_lbl, "Trigger:");
        snprintf(set, sizeof(set), "Rising > 0");
    } else {
        lv_label_set_text(s_set_row_lbl, "Maximum:");
        fmt_threshold(set, sizeof(set), id, true);
    }
    lv_label_set_text(s_set_val_lbl, set);

    char ack_txt[48];
    snprintf(ack_txt, sizeof(ack_txt),
             LV_SYMBOL_OK "  ACK ΓÇö re-arm in %d min",
             gSettings.alarm_rearm_minutes);
    lv_label_set_text(s_ack_btn_lbl, ack_txt);
}

// ΓöÇΓöÇ Init ΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇ
void alarm_overlay_init(void) {
    // lv_layer_top() always renders above everything
    // and receives touch before the tabview
    s_overlay = lv_obj_create(lv_layer_top());
    lv_obj_set_size(s_overlay, 480, 480);
    lv_obj_set_pos(s_overlay, 0, 0);
    lv_obj_set_style_bg_color(s_overlay, lv_color_hex(0x080C14), 0);
    lv_obj_set_style_bg_opa(s_overlay, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_overlay, 0, 0);
    lv_obj_set_scrollbar_mode(s_overlay, LV_SCROLLBAR_MODE_OFF);
    lv_obj_add_flag(s_overlay, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(s_overlay, LV_OBJ_FLAG_SCROLLABLE);
    // Consume touch events ΓÇö prevent them reaching tabview underneath
    lv_obj_add_event_cb(s_overlay, [](lv_event_t*) {}, LV_EVENT_CLICKED, NULL);

    // Warning icon + title
    s_icon_lbl = lv_label_create(s_overlay);
    lv_label_set_text(s_icon_lbl, LV_SYMBOL_WARNING " ALARM");
    lv_obj_set_style_text_font(s_icon_lbl, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(s_icon_lbl, C_RED, 0);
    lv_obj_align(s_icon_lbl, LV_ALIGN_TOP_MID, 0, 24);

    s_title_lbl = lv_label_create(s_overlay);
    lv_label_set_text(s_title_lbl, "");
    lv_obj_set_style_text_font(s_title_lbl, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(s_title_lbl, C_TEXT_PRI, 0);
    lv_obj_align(s_title_lbl, LV_ALIGN_TOP_MID, 0, 60);

    // Divider
    lv_obj_t* div = lv_obj_create(s_overlay);
    lv_obj_set_size(div, 400, 2);
    lv_obj_set_style_bg_color(div, C_BORDER, 0);
    lv_obj_set_style_bg_opa(div, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(div, 0, 0);
    lv_obj_align(div, LV_ALIGN_TOP_MID, 0, 100);

    // Current value row
    s_cur_row_lbl = lv_label_create(s_overlay);
    lv_label_set_text(s_cur_row_lbl, "Current:");
    lv_obj_set_style_text_color(s_cur_row_lbl, C_TEXT_SEC, 0);
    lv_obj_set_style_text_font(s_cur_row_lbl, &lv_font_montserrat_16, 0);
    lv_obj_set_pos(s_cur_row_lbl, 60, 116);

    s_cur_val_lbl = lv_label_create(s_overlay);
    lv_label_set_text(s_cur_val_lbl, "---");
    lv_obj_set_style_text_color(s_cur_val_lbl, C_RED, 0);
    lv_obj_set_style_text_font(s_cur_val_lbl, &lv_font_montserrat_32, 0);
    lv_obj_set_pos(s_cur_val_lbl, 60, 138);

    // Setpoint row
    s_set_row_lbl = lv_label_create(s_overlay);
    lv_label_set_text(s_set_row_lbl, "Setpoint:");
    lv_obj_set_style_text_color(s_set_row_lbl, C_TEXT_SEC, 0);
    lv_obj_set_style_text_font(s_set_row_lbl, &lv_font_montserrat_16, 0);
    lv_obj_set_pos(s_set_row_lbl, 60, 190);

    s_set_val_lbl = lv_label_create(s_overlay);
    lv_label_set_text(s_set_val_lbl, "---");
    lv_obj_set_style_text_color(s_set_val_lbl, C_ACCENT, 0);
    lv_obj_set_style_text_font(s_set_val_lbl, &lv_font_montserrat_24, 0);
    lv_obj_set_pos(s_set_val_lbl, 60, 212);

    // Divider 2
    lv_obj_t* div2 = lv_obj_create(s_overlay);
    lv_obj_set_size(div2, 400, 2);
    lv_obj_set_style_bg_color(div2, C_BORDER, 0);
    lv_obj_set_style_bg_opa(div2, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(div2, 0, 0);
    lv_obj_align(div2, LV_ALIGN_TOP_MID, 0, 258);

    // ACK button
    lv_obj_t* ack_btn = lv_btn_create(s_overlay);
    lv_obj_set_size(ack_btn, 400, 60);
    lv_obj_align(ack_btn, LV_ALIGN_TOP_MID, 0, 274);
    lv_obj_set_style_bg_color(ack_btn, C_ACCENT, 0);
    lv_obj_set_style_radius(ack_btn, 8, 0);
    s_ack_btn_lbl = lv_label_create(ack_btn);
    lv_label_set_text(s_ack_btn_lbl, LV_SYMBOL_OK "  ACK");
    lv_obj_set_style_text_font(s_ack_btn_lbl, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(s_ack_btn_lbl, lv_color_hex(0x000000), 0);
    lv_obj_center(s_ack_btn_lbl);
    lv_obj_add_event_cb(ack_btn, [](lv_event_t*) {
        alarm_acknowledge();
        // Hide deferred to alarm_overlay_update() next tick
    }, LV_EVENT_CLICKED, NULL);

    // SILENCE button
    lv_obj_t* sil_btn = lv_btn_create(s_overlay);
    lv_obj_set_size(sil_btn, 400, 60);
    lv_obj_align(sil_btn, LV_ALIGN_TOP_MID, 0, 348);
    lv_obj_set_style_bg_color(sil_btn, lv_color_hex(0x401010), 0);
    lv_obj_set_style_radius(sil_btn, 8, 0);
    lv_obj_t* sil_lbl = lv_label_create(sil_btn);
    lv_label_set_text(sil_lbl, LV_SYMBOL_MUTE "  SILENCE ALARM");
    lv_obj_set_style_text_font(sil_lbl, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(sil_lbl, C_TEXT_PRI, 0);
    lv_obj_center(sil_lbl);
    lv_obj_add_event_cb(sil_btn, [](lv_event_t*) {
        alarm_silence();
        // Hide deferred to alarm_overlay_update() next tick
    }, LV_EVENT_CLICKED, NULL);

    // Start hidden
    lv_obj_add_flag(s_overlay, LV_OBJ_FLAG_HIDDEN);

    ESP_LOGI(TAG, "Alarm overlay initialised");
}

// ΓöÇΓöÇ Update ΓÇö called from ui_update() every tick ΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇ
void alarm_overlay_update(void) {
    AlarmID active_id;
    bool has_unacked = find_active_alarm(&active_id);

    if (has_unacked) {
        if (!s_visible || s_shown_id != active_id) {
            s_visible  = true;
            s_shown_id = active_id;
            lv_obj_clear_flag(s_overlay, LV_OBJ_FLAG_HIDDEN);
            refresh(active_id);
        }
    } else if (s_visible) {
        lv_obj_add_flag(s_overlay, LV_OBJ_FLAG_HIDDEN);
        s_visible  = false;
        s_shown_id = ALARM_COUNT;
    }
}
