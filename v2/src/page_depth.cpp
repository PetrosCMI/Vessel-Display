// page_depth.cpp — Depth & environment page
#include "page_registry.h"
#include "ui.h"
#include "boat_data.h"
#include "units.h"
#include "alarm.h"
#include <lvgl.h>

static InstrCard dep_depth, dep_water_temp, dep_air_temp;

static void build(lv_obj_t* tab) {
    dep_depth = make_instr_card(tab, "DEPTH", "m", true);
    lv_obj_set_size(dep_depth.card, 454, 180);
    lv_obj_set_pos(dep_depth.card, 0, 0);
    // Override value colour to teal (will be overridden to red on alarm)
    lv_obj_set_style_text_color(dep_depth.val_lbl, C_DEPTH, 0);

    dep_water_temp = make_instr_card(tab, "WATER TEMP", "°C", false);
    lv_obj_set_size(dep_water_temp.card, 222, 140);
    lv_obj_set_pos(dep_water_temp.card, 0, 190);

    dep_air_temp = make_instr_card(tab, "AIR TEMP", "°C", false);
    lv_obj_set_size(dep_air_temp.card, 222, 140);
    lv_obj_set_pos(dep_air_temp.card, 232, 190);
}

static void update() {
    BoatData d = boatDataSnapshot();
    char buf[24];

    bool depth_lo = alarm_is_active(ALARM_DEPTH_MIN);
    bool depth_hi = alarm_is_active(ALARM_DEPTH_MAX);
    const char* du = fmt_depth(d.depth_m, buf, sizeof(buf));
    instr_card_set(dep_depth, buf, du, depth_hi, depth_lo);  // lo = critical (red)
    if (!depth_lo && !depth_hi)
        lv_obj_set_style_text_color(dep_depth.val_lbl, C_DEPTH, 0);

    const char* tu = fmt_temp(d.water_temp_c, buf, sizeof(buf));
    instr_card_set(dep_water_temp, buf, tu);

    tu = fmt_temp(d.air_temp_c, buf, sizeof(buf));
    instr_card_set(dep_air_temp, buf, tu);
}

static struct DepthPageRegistrar {
    DepthPageRegistrar() {
        ui_register_page({ LV_SYMBOL_DOWN " DEPTH", build, update });
    }
} _depth_reg;
