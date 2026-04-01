// page_depth.cpp — Depth & environment page
#include "page_registry.h"
#include "ui.h"
#include "boat_data.h"
#include "units.h"
#include "alarm.h"
#include "lvgl.h"

// ── Layout (454 × 370 usable) ────────────────────────────────
//
//  Top 2/3 (~247px):
//    Left 1/2 (227px): DEPTH (full height)
//    Right 1/2 (227px): MAX (top half) / MIN + RST (bottom half)
//
//  Bottom 1/3 (~123px):
//    Left 1/2: WATER TEMP
//    Right 1/2: AIR TEMP

static InstrCard dep_depth, dep_max, dep_min;
static InstrCard dep_water_temp, dep_air_temp;

static float last_depth = 0;
static float last_water_temp = 0;
static float last_air_temp = 0;

static const int W  = 454;
static const int H  = 370;
static const int HW = 227;   // half width
static const int TH = 247;   // top 2/3 height
static const int BH = 120;   // bottom 1/3 height
static const int MH = 122;   // half of top height (for MAX/MIN)

static void build(lv_obj_t* tab) {
    // DEPTH — left half, full 2/3 height
    dep_depth = make_instr_card(tab, "DEPTH", "m", true);
    lv_obj_set_size(dep_depth.card, HW - 3, TH);
    lv_obj_set_pos(dep_depth.card, 0, 0);
    lv_obj_set_style_text_color(dep_depth.val_lbl, C_DEPTH, 0);

    // MAX — right half, top portion
    dep_max = make_instr_card(tab, "MAX", "m", false);
    lv_obj_set_size(dep_max.card, HW - 3, MH - 2);
    lv_obj_set_pos(dep_max.card, HW + 3, 0);
    lv_obj_set_style_text_color(dep_max.val_lbl, C_TEXT_SEC, 0);

    // MIN — right half, bottom portion with RST button
    dep_min = make_instr_card(tab, "MIN", "m", false);
    lv_obj_set_size(dep_min.card, HW - 3, MH - 2);
    lv_obj_set_pos(dep_min.card, HW + 3, MH + 2);
    lv_obj_set_style_text_color(dep_min.val_lbl, C_TEXT_SEC, 0);

    // RST button inside MIN card
    lv_obj_t* rst = lv_btn_create(dep_min.card);
    lv_obj_set_size(rst, 56, 56);
    lv_obj_align(rst, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
    lv_obj_set_style_bg_color(rst, lv_color_hex(0x303040), 0);
    lv_obj_set_style_radius(rst, 4, 0);
    lv_obj_t* rl = lv_label_create(rst);
    lv_label_set_text(rl, "RST");
    lv_obj_set_style_text_font(rl, &lv_font_montserrat_12, 0);
    lv_obj_center(rl);
    lv_obj_add_event_cb(rst, [](lv_event_t*) {
        boatResetDepthMinMax();
    }, LV_EVENT_CLICKED, NULL);

    // WATER TEMP — bottom left
    dep_water_temp = make_instr_card(tab, "WATER TEMP", "\xc2\xb0""C", false);
    lv_obj_set_size(dep_water_temp.card, HW - 3, BH);
    lv_obj_set_pos(dep_water_temp.card, 0, TH + 3);

    // AIR TEMP — bottom right
    dep_air_temp = make_instr_card(tab, "AIR TEMP", "\xc2\xb0""C", false);
    lv_obj_set_size(dep_air_temp.card, HW - 3, BH);
    lv_obj_set_pos(dep_air_temp.card, HW + 3, TH + 3);
}

static void update(void) {
    BoatData d = boatDataSnapshot();
    char buf[24];
    char val_buf[16];  // Buffer for the numeric value only

    bool depth_lo = alarm_is_active(ALARM_DEPTH_MIN);
    bool depth_hi = alarm_is_active(ALARM_DEPTH_MAX);

    // 1. Get the formatted depth string (e.g., "12.4")
    const char* du = fmt_depth(d.depth_m, val_buf, sizeof(val_buf));

    // 2. Combine value with the trend arrow/minus
    snprintf(buf, sizeof(buf), "%s %s", val_buf, get_trend_symbol(d.depth_m, last_depth));
    last_depth = d.depth_m;

    // 3. Update the card
    instr_card_set(dep_depth, buf, du, depth_hi, depth_lo);
    if (!depth_lo && !depth_hi)
        lv_obj_set_style_text_color(dep_depth.val_lbl, C_DEPTH, 0);

    du = fmt_depth(d.depth_max_m, buf, sizeof(buf));
    instr_card_set(dep_max, buf, du);

    du = fmt_depth(d.depth_min_m, buf, sizeof(buf));
    instr_card_set(dep_min, buf, du);

    const char* tu = fmt_temp(d.water_temp_c, val_buf, sizeof(buf));
    snprintf(buf, sizeof(buf), "%s %s", val_buf, get_trend_symbol(d.water_temp_c, last_water_temp));
    last_water_temp = d.water_temp_c;
    instr_card_set(dep_water_temp, buf, tu);

    tu = fmt_temp(d.air_temp_c, val_buf, sizeof(buf));
    snprintf(buf, sizeof(buf), "%s %s", val_buf, get_trend_symbol(d.air_temp_c, last_air_temp));
    last_air_temp = d.air_temp_c;
    instr_card_set(dep_air_temp, buf, tu);
}

static struct DepthReg {
    DepthReg() { ui_register_page({ LV_SYMBOL_DOWN " DPTH", build, update }); }
} _reg;
