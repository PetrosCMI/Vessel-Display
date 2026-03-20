// page_wind.cpp — Wind page
#include "page_registry.h"
#include "ui.h"
#include "boat_data.h"
#include "units.h"
#include "alarm.h"
#include "lvgl.h"

static InstrCard wind_aws, wind_tws, wind_twd;
static lv_obj_t* wind_arc;
static lv_obj_t* wind_awa_lbl;
static lv_obj_t* wind_twa_lbl;

static void build_rose(lv_obj_t* parent, int x, int y, int sz) {
    lv_obj_t* c = lv_obj_create(parent);
    lv_obj_add_style(c, &g_style_card, 0);
    lv_obj_set_size(c, sz, sz);
    lv_obj_set_pos(c, x, y);
    lv_obj_set_scrollbar_mode(c, LV_SCROLLBAR_MODE_OFF);

    lv_obj_t* title = lv_label_create(c);
    lv_label_set_text(title, "WIND ANGLE");
    lv_obj_add_style(title, &g_style_label, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 0);

    wind_arc = lv_arc_create(c);
    lv_arc_set_range(wind_arc, -180, 180);
    lv_arc_set_value(wind_arc, 0);
    lv_arc_set_bg_angles(wind_arc, 0, 360);
    lv_obj_set_size(wind_arc, sz - 36, sz - 36);
    lv_obj_center(wind_arc);
    lv_obj_set_style_arc_color(wind_arc, C_BORDER, LV_PART_MAIN);
    lv_obj_set_style_arc_color(wind_arc, C_WIND,   LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(wind_arc, 5, LV_PART_MAIN);
    lv_obj_set_style_arc_width(wind_arc, 5, LV_PART_INDICATOR);
    lv_obj_remove_style(wind_arc, NULL, LV_PART_KNOB);
    lv_obj_clear_flag(wind_arc, LV_OBJ_FLAG_CLICKABLE);

    wind_awa_lbl = lv_label_create(c);
    lv_label_set_text(wind_awa_lbl, "AWA ---\xc2\xb0");
    lv_obj_add_style(wind_awa_lbl, &g_style_label, 0);
    lv_obj_align(wind_awa_lbl, LV_ALIGN_CENTER, 0, -10);

    wind_twa_lbl = lv_label_create(c);
    lv_label_set_text(wind_twa_lbl, "TWA ---\xc2\xb0");
    lv_obj_add_style(wind_twa_lbl, &g_style_label, 0);
    lv_obj_align(wind_twa_lbl, LV_ALIGN_CENTER, 0, 10);
}

static void build(lv_obj_t* tab) {
    build_rose(tab, 0, 0, 220);

    wind_aws = make_instr_card(tab, "AWS", "kts", true);
    lv_obj_set_size(wind_aws.card, 226, 100);
    lv_obj_set_pos(wind_aws.card, 228, 0);

    wind_tws = make_instr_card(tab, "TWS", "kts", true);
    lv_obj_set_size(wind_tws.card, 226, 100);
    lv_obj_set_pos(wind_tws.card, 228, 108);

    wind_twd = make_instr_card(tab, "TWD", "\xc2\xb0""T", false);
    lv_obj_set_size(wind_twd.card, 454, 100);
    lv_obj_set_pos(wind_twd.card, 0, 228);
}

static void update(void) {
    BoatData d = boatDataSnapshot();
    char buf[24];

    bool wind_alarm = alarm_is_active(ALARM_WIND_SPEED_MAX);
    const char* su = fmt_speed(d.aws_ms, buf, sizeof(buf));
    instr_card_set(wind_aws, buf, su, false, wind_alarm);

    su = fmt_speed(d.tws_ms, buf, sizeof(buf));
    instr_card_set(wind_tws, buf, su);

    fmt_angle(d.twd_deg, buf, sizeof(buf));
    instr_card_set(wind_twd, buf, "\xc2\xb0""T");

    if (!isnan(d.awa_deg)) {
        lv_arc_set_value(wind_arc, (int)d.awa_deg);
        snprintf(buf, sizeof(buf), "AWA %+.0f\xc2\xb0", d.awa_deg);
        lv_label_set_text(wind_awa_lbl, buf);
    }
    if (!isnan(d.twa_deg)) {
        snprintf(buf, sizeof(buf), "TWA %+.0f\xc2\xb0", d.twa_deg);
        lv_label_set_text(wind_twa_lbl, buf);
    }
}

static struct WindReg {
    WindReg() { ui_register_page({ LV_SYMBOL_UP " WIND", build, update }); }
} _reg;
