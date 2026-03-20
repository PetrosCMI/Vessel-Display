// page_engine.cpp — Engine & electrical page
#include "page_registry.h"
#include "ui.h"
#include "boat_data.h"
#include "units.h"
#include "lvgl.h"

static InstrCard eng_rpm, eng_coolant, eng_oil, eng_batt, eng_alt;

static void build(lv_obj_t* tab) {
    eng_rpm = make_instr_card(tab, "ENGINE RPM", "rpm", true);
    lv_obj_set_size(eng_rpm.card, 454, 120);
    lv_obj_set_pos(eng_rpm.card, 0, 0);

    eng_coolant = make_instr_card(tab, "COOLANT", "\xc2\xb0""C", false);
    lv_obj_set_size(eng_coolant.card, 148, 110);
    lv_obj_set_pos(eng_coolant.card, 0, 130);

    eng_oil = make_instr_card(tab, "OIL", "kPa", false);
    lv_obj_set_size(eng_oil.card, 148, 110);
    lv_obj_set_pos(eng_oil.card, 156, 130);

    eng_batt = make_instr_card(tab, "HOUSE V", "V", false);
    lv_obj_set_size(eng_batt.card, 148, 110);
    lv_obj_set_pos(eng_batt.card, 0, 250);

    eng_alt = make_instr_card(tab, "START V", "V", false);
    lv_obj_set_size(eng_alt.card, 148, 110);
    lv_obj_set_pos(eng_alt.card, 156, 250);
}

static void update(void) {
    BoatData d = boatDataSnapshot();
    char buf[24];

    fmt_rpm(d.rpm, buf, sizeof(buf));
    instr_card_set(eng_rpm, buf, "rpm",
        d.rpm > 3000.0f, d.rpm > 3500.0f);

    const char* tu = fmt_temp(d.coolant_temp, buf, sizeof(buf));
    instr_card_set(eng_coolant, buf, tu,
        d.coolant_temp > 90.0f, d.coolant_temp > 95.0f);

    const char* pu = fmt_pressure(d.oil_pressure, buf, sizeof(buf));
    instr_card_set(eng_oil, buf, pu,
        d.oil_pressure < 200.0f, d.oil_pressure < 150.0f);

    fmt_volts(d.battery_v, buf, sizeof(buf));
    instr_card_set(eng_batt, buf, "V",
        d.battery_v < 12.2f || d.battery_v > 14.8f,
        d.battery_v < 11.8f);

    fmt_volts(d.start_v, buf, sizeof(buf));
    instr_card_set(eng_alt, buf, "V",
        d.start_v < 12.2f || d.start_v > 14.8f,
        d.start_v < 11.8f);
}

static struct EngReg {
    EngReg() { ui_register_page({ LV_SYMBOL_SETTINGS " ENG", build, update }); }
} _reg;
