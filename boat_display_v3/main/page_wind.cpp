// page_wind.cpp ΓÇö Wind page
#include "page_registry.h"
#include "ui.h"
#include "boat_data.h"
#include "units.h"
#include "alarm.h"
#include "lvgl.h"

// ΓöÇΓöÇ Layout (454 ├ù 370 usable) ΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇ
//
//  Top 50% (~185px):
//    Left 1/2: AWS (top 60%) + AWA (bottom 40%)
//    Right 1/2: MAX AWS + RST button
//
//  Bottom 50% (~185px):
//    Left 1/2: PRESSURE
//    Right 1/2: STORM LEVEL

static InstrCard wind_aws, wind_awa, wind_aws_max;
static InstrCard wind_pressure, wind_storm;

static const int HH = 185;
static const int HW = 225;
static float last_aws = 0;
static float last_pressure = 0;
static float last_storm = 0;

static void build(lv_obj_t* tab) {
    // AWS ΓÇö top-left, standard size
    wind_aws = make_instr_card(tab, "AWS", "kts", false);
    //lv_obj_align(wind_aws, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_set_size(wind_aws.card, HW, 108);
    lv_obj_set_pos(wind_aws.card, 0, 0);

    // AWA ΓÇö bottom 40% of top-left
    wind_awa = make_instr_card(tab, "AWA", "\xc2\xb0", false);
    lv_obj_set_size(wind_awa.card, HW, 73);
    lv_obj_set_pos(wind_awa.card, 0, 112);

    // AWS MAX ΓÇö top-right, large value
    wind_aws_max = make_instr_card(tab, "MAX AWS", "kts", true);
    lv_obj_set_size(wind_aws_max.card, HW - 2, HH - 2);
    lv_obj_set_pos(wind_aws_max.card, HW + 4, 0);
    lv_obj_set_style_text_color(wind_aws_max.val_lbl, C_TEXT_PRI, 0);

    // RST button inside AWS MAX card ΓÇö square
    lv_obj_t* rst = lv_btn_create(wind_aws_max.card);
    lv_obj_set_size(rst, 56, 56);
    lv_obj_align(rst, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
    lv_obj_set_style_bg_color(rst, lv_color_hex(0x303040), 0);
    lv_obj_set_style_radius(rst, 4, 0);
    lv_obj_t* rl = lv_label_create(rst);
    lv_label_set_text(rl, "RST");
    lv_obj_set_style_text_font(rl, &lv_font_montserrat_12, 0);
    lv_obj_center(rl);
    lv_obj_add_event_cb(rst, [](lv_event_t*) {
        boatResetWindMax();
    }, LV_EVENT_CLICKED, NULL);

    // PRESSURE ΓÇö bottom left
    wind_pressure = make_instr_card(tab, "PRESSURE", "hPa", true);
    lv_obj_set_size(wind_pressure.card, HW, HH - 2);
    lv_obj_set_pos(wind_pressure.card, 0, HH + 2);

    // STORM LEVEL ΓÇö bottom right
    wind_storm = make_instr_card(tab, "STORM LVL", "", true);
    lv_obj_set_size(wind_storm.card, HW - 2, HH - 2);
    lv_obj_set_pos(wind_storm.card, HW + 4, HH + 2);
}

static void update(void) {
    BoatData d = boatDataSnapshot();
    char buf[32]; // Increased size to accommodate symbols
    char val_buf[16];

    bool wind_alarm = alarm_is_active(ALARM_WIND_SPEED_MAX);
    const char* su = fmt_speed(d.aws_ms, val_buf, sizeof(val_buf));
    // Combine value and arrow
    snprintf(buf, sizeof(buf), "%s %s", val_buf, get_trend_symbol(d.aws_ms, last_aws));
    instr_card_set(wind_aws, buf, su, false, wind_alarm);
    last_aws = d.aws_ms;

    if (!isnan(d.awa_deg)) {
        snprintf(buf, sizeof(buf), "%+.0f", d.awa_deg);
        instr_card_set(wind_awa, buf, "\xc2\xb0");
    } else {
        instr_card_set(wind_awa, "---", "\xc2\xb0");
    }

    su = fmt_speed(d.aws_max_ms, buf, sizeof(buf));
    instr_card_set(wind_aws_max, buf, su);

    // --- PRESSURE with Trend ---
    if (!isnan(d.pressure_hpa)) {
        snprintf(val_buf, sizeof(val_buf), "%.1f", d.pressure_hpa);
        snprintf(buf, sizeof(buf), "%s %s", val_buf, get_trend_symbol(d.pressure_hpa, last_pressure));
        instr_card_set(wind_pressure, buf, "hPa");
        last_pressure = d.pressure_hpa;
    } else {
        instr_card_set(wind_pressure, "---", "hPa");
    }

    // --- STORM LEVEL with Trend ---
    bool storm_alarm = alarm_is_active(ALARM_STORM);
    if (!isnan(d.storm_level)) {
        snprintf(val_buf, sizeof(val_buf), "%.0f", d.storm_level);
        snprintf(buf, sizeof(buf), "%s %s", val_buf, get_trend_symbol(d.storm_level, last_storm));
        instr_card_set(wind_storm, buf, "", storm_alarm, false);
        
        lv_obj_set_style_text_color(wind_storm.val_lbl,
            storm_alarm ? C_RED :
            d.storm_level > 0 ? C_YELLOW : C_TEXT_PRI, 0);
        last_storm = d.storm_level;
    } else {
        instr_card_set(wind_storm, "---", "");
    }
}

static struct WindReg {
    WindReg() { ui_register_page({ LV_SYMBOL_UP " WX", build, update }); }
} _reg;
