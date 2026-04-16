// page_engine.cpp ΓÇö Engine page
#include "page_registry.h"
#include "ui.h"
#include "boat_data.h"
#include "units.h"
#include "lvgl.h"

static InstrCard eng_rpm, eng_coolant, eng_oil;

static void build(lv_obj_t* tab) {
    eng_rpm = make_instr_card(tab, "ENGINE RPM", "rpm", true);
    lv_obj_set_size(eng_rpm.card, 454, 160);
    lv_obj_set_pos(eng_rpm.card, 0, 0);

    eng_coolant = make_instr_card(tab, "COOLANT", "\xc2\xb0""C", false);
    lv_obj_set_size(eng_coolant.card, 222, 180);
    lv_obj_set_pos(eng_coolant.card, 0, 170);

    eng_oil = make_instr_card(tab, "OIL PRESSURE", "kPa", false);
    lv_obj_set_size(eng_oil.card, 222, 180);
    lv_obj_set_pos(eng_oil.card, 232, 170);
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
}

static struct EngReg {
    EngReg() { ui_register_page({ LV_SYMBOL_SETTINGS " ENG", build, update }); }
} _reg;
