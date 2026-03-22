// page_wind.cpp — Wind page
#include "page_registry.h"
#include "ui.h"
#include "boat_data.h"
#include "units.h"
#include "alarm.h"
#include "lvgl.h"

// ── Layout (454 × 370 usable) ────────────────────────────────
//
//  Top 50% (~185px):
//    Left 1/2: AWS (top 60%) + AWA (bottom 40%)
//    Right 1/2: AWS MAX + RST button
//
//  Bottom 50% (~185px):
//    Three equal cards: TWS | TWA | TWD

static InstrCard wind_aws, wind_awa, wind_aws_max;
static InstrCard wind_tws, wind_twa, wind_twd;

static const int HH  = 185;   // half height
static const int HW  = 225;   // half width
static const int TW  = 148;   // third width

static void build(lv_obj_t* tab) {
    // AWS — top-left, standard size
    wind_aws = make_instr_card(tab, "AWS", "kts", false);
    lv_obj_set_size(wind_aws.card, HW, 108);
    lv_obj_set_pos(wind_aws.card, 0, 0);

    // AWA — bottom 40% of top-left
    wind_awa = make_instr_card(tab, "AWA", "\xc2\xb0", false);
    lv_obj_set_size(wind_awa.card, HW, 73);
    lv_obj_set_pos(wind_awa.card, 0, 112);

    // AWS MAX — top-right, large value
    wind_aws_max = make_instr_card(tab, "MAX AWS", "kts", true);
    lv_obj_set_size(wind_aws_max.card, HW - 2, HH - 2);
    lv_obj_set_pos(wind_aws_max.card, HW + 4, 0);
    lv_obj_set_style_text_color(wind_aws_max.val_lbl, C_TEXT_PRI, 0);

    // RST button inside AWS MAX card — square
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

    // Bottom row: TWS | TWA | TWD
    wind_tws = make_instr_card(tab, "TWS", "kts", false);
    lv_obj_set_size(wind_tws.card, TW, HH - 2);
    lv_obj_set_pos(wind_tws.card, 0, HH + 2);

    wind_twa = make_instr_card(tab, "TWA", "\xc2\xb0", false);
    lv_obj_set_size(wind_twa.card, TW, HH - 2);
    lv_obj_set_pos(wind_twa.card, TW + 3, HH + 2);

    wind_twd = make_instr_card(tab, "TWD", "\xc2\xb0""T", false);
    lv_obj_set_size(wind_twd.card, TW, HH - 2);
    lv_obj_set_pos(wind_twd.card, (TW + 3) * 2, HH + 2);
}

static void update(void) {
    BoatData d = boatDataSnapshot();
    char buf[24];

    bool wind_alarm = alarm_is_active(ALARM_WIND_SPEED_MAX);
    const char* su = fmt_speed(d.aws_ms, buf, sizeof(buf));
    instr_card_set(wind_aws, buf, su, false, wind_alarm);

    if (!isnan(d.awa_deg)) {
        snprintf(buf, sizeof(buf), "%+.0f", d.awa_deg);
        instr_card_set(wind_awa, buf, "\xc2\xb0");
    } else {
        instr_card_set(wind_awa, "---", "\xc2\xb0");
    }

    su = fmt_speed(d.aws_max_ms, buf, sizeof(buf));
    instr_card_set(wind_aws_max, buf, su);

    su = fmt_speed(d.tws_ms, buf, sizeof(buf));
    instr_card_set(wind_tws, buf, su);

    if (!isnan(d.twa_deg)) {
        snprintf(buf, sizeof(buf), "%+.0f", d.twa_deg);
        instr_card_set(wind_twa, buf, "\xc2\xb0");
    } else {
        instr_card_set(wind_twa, "---", "\xc2\xb0");
    }

    fmt_angle(d.twd_deg, buf, sizeof(buf));
    instr_card_set(wind_twd, buf, "\xc2\xb0""T");
}

static struct WindReg {
    WindReg() { ui_register_page({ LV_SYMBOL_UP " WIND", build, update }); }
} _reg;