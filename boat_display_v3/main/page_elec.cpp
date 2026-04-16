// page_elec.cpp — Electrical / battery page
#include "page_registry.h"
#include "ui.h"
#include "boat_data.h"
#include "lvgl.h"
#include "esp_log.h"
#include <stdio.h>
#include <math.h>

// ── Layout (480×390) ─────────────────────────────────────────
//
//  Top Row    (h=190): [ HOUSE (left half)  ] [ HOUSE LI (right half) ]
//  Bottom Row (h=190): [ START (left half)  ] [ FORWARD (right half)  ]

// ── Widget refs ───────────────────────────────────────────────
struct BankCard {
    lv_obj_t* v_lbl;
    lv_obj_t* a_lbl;
    lv_obj_t* soc_lbl;
};

static BankCard s_house, s_li, s_start, s_fwd;

static void fmt_v(float v, char* buf, size_t n) {
    if (isnan(v)) snprintf(buf, n, "---");
    else          snprintf(buf, n, "%.2f V", v);
}

// ── Helper: format current (+ = charging, - = discharging) ───
static void fmt_a(float a, char* buf, size_t n) {
    if (isnan(a)) snprintf(buf, n, "--- A");
    else          snprintf(buf, n, "%+.2f A", a);
}

// ── Helper: format SOC ───────────────────────────────────────
static void fmt_soc(float soc, char* buf, size_t n) {
    if (isnan(soc)) snprintf(buf, n, "--%%");
    else            snprintf(buf, n, "%.0f%%", soc);
}

// ── Helper: current label colour ─────────────────────────────
static lv_color_t current_colour(float a) {
    if (isnan(a))  return C_TEXT_SEC;
    if (a > 0.5f)  return C_GREEN;    // charging
    if (a < -0.5f) return C_YELLOW;   // discharging
    return C_TEXT_SEC;                 // idle
}

// ── Helper: voltage label colour ─────────────────────────────
static lv_color_t voltage_colour(float v) {
    if (isnan(v))   return C_TEXT_SEC;
    if (v >= 13.0f) return C_GREEN;
    if (v >= 12.4f) return C_TEXT_PRI;
    if (v >= 12.0f) return C_YELLOW;
    return C_RED;
}

// ── Helper: SOC label colour ─────────────────────────────────
static lv_color_t soc_colour(float soc) {
    if (isnan(soc))  return C_TEXT_SEC;
    if (soc >= 80.0f) return C_GREEN;
    if (soc >= 50.0f) return C_TEXT_PRI;
    if (soc >= 20.0f) return C_YELLOW;
    return C_RED;
}

// ── Make a bank card ─────────────────────────────────────────
static BankCard make_bank_card(lv_obj_t* parent,
                                const char* name,
                                bool has_current,
                                int x, int y, int w, int h) {
    BankCard bc = {};
    ESP_LOGD( "ELEC", "Making bank card %s", name );

    lv_obj_t* card = lv_obj_create(parent);
    lv_obj_add_style(card, &g_style_card, 0);
    lv_obj_set_size(card, w, h);
    lv_obj_set_pos(card, x, y);
    lv_obj_set_scrollbar_mode(card, LV_SCROLLBAR_MODE_OFF);

    // Bank name
    lv_obj_t* name_lbl = lv_label_create(card);
    lv_label_set_text(name_lbl, name);
    lv_obj_add_style(name_lbl, &g_style_label, 0);
    lv_obj_set_pos(name_lbl, 0, 0);

    // Voltage — large, prominent
    bc.v_lbl = lv_label_create(card);
    lv_label_set_text(bc.v_lbl, "---");
    lv_obj_set_style_text_font(bc.v_lbl, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(bc.v_lbl, C_TEXT_PRI, 0);
    lv_obj_set_pos(bc.v_lbl, 0, 22);

    // Current (left side, bottom)
    if (has_current) {
        lv_obj_t* a_name = lv_label_create(card);
        lv_label_set_text(a_name, "Current");
        lv_obj_add_style(a_name, &g_style_label, 0);
        lv_obj_set_pos(a_name, 0, 96);

        bc.a_lbl = lv_label_create(card);
        lv_label_set_text(bc.a_lbl, "---");
        lv_obj_set_style_text_font(bc.a_lbl, &lv_font_montserrat_24, 0);
        lv_obj_set_style_text_color(bc.a_lbl, C_TEXT_SEC, 0);
        lv_obj_set_pos(bc.a_lbl, 0, 116);
    }

    // SOC (right side, bottom)
    lv_obj_t* soc_name = lv_label_create(card);
    lv_label_set_text(soc_name, "SOC");
    lv_obj_add_style(soc_name, &g_style_label, 0);
    lv_obj_set_pos(soc_name, has_current ? 120 : 0, 96);

    bc.soc_lbl = lv_label_create(card);
    lv_label_set_text(bc.soc_lbl, "--");
    lv_obj_set_style_text_font(bc.soc_lbl, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(bc.soc_lbl, C_TEXT_SEC, 0);
    lv_obj_set_pos(bc.soc_lbl, has_current ? 120 : 0, 116);

    ui_make_scroll_transparent(card);
    return bc;
}

static void build(lv_obj_t* tab) {

    // Top left: HOUSE
    s_house = make_bank_card(tab, "HOUSE", true, 0, 0, 227, 190);

    // Top right: HOUSE LI
    s_li = make_bank_card(tab, "HOUSE LI", true, 234, 0, 227, 190);

    // Bottom left: START
    s_start = make_bank_card(tab, "START", true, 0, 197, 227, 190);

    // Bottom right: FORWARD
    s_fwd = make_bank_card(tab, "FORWARD", true, 234, 197, 227, 190);
}

static void update(void) {
    BoatData d = boatDataSnapshot();
    char buf[24];

    // House (all batteries combined)
    fmt_v(d.house_v, buf, sizeof(buf));
    lv_label_set_text(s_house.v_lbl, buf);
    lv_obj_set_style_text_color(s_house.v_lbl, voltage_colour(d.house_v), 0);

    fmt_a(d.house_a, buf, sizeof(buf));
    lv_label_set_text(s_house.a_lbl, buf);
    lv_obj_set_style_text_color(s_house.a_lbl, current_colour(d.house_a), 0);

    fmt_soc(d.house_soc, buf, sizeof(buf));
    lv_label_set_text(s_house.soc_lbl, buf);
    lv_obj_set_style_text_color(s_house.soc_lbl, soc_colour(d.house_soc), 0);

    // LI (lithium battery)
    fmt_v(d.house_v_li, buf, sizeof(buf));
    lv_label_set_text(s_li.v_lbl, buf);
    lv_obj_set_style_text_color(s_li.v_lbl, voltage_colour(d.house_v_li), 0);

    fmt_a(d.house_a_li, buf, sizeof(buf));
    lv_label_set_text(s_li.a_lbl, buf);
    lv_obj_set_style_text_color(s_li.a_lbl, current_colour(d.house_a_li), 0);

    fmt_soc(d.house_li_soc, buf, sizeof(buf));
    lv_label_set_text(s_li.soc_lbl, buf);
    lv_obj_set_style_text_color(s_li.soc_lbl, soc_colour(d.house_li_soc), 0);

    // Start
    fmt_v(d.start_batt_v, buf, sizeof(buf));
    lv_label_set_text(s_start.v_lbl, buf);
    lv_obj_set_style_text_color(s_start.v_lbl, voltage_colour(d.start_batt_v), 0);

    // Forward
    fmt_v(d.forward_v, buf, sizeof(buf));
    lv_label_set_text(s_fwd.v_lbl, buf);
    lv_obj_set_style_text_color(s_fwd.v_lbl, voltage_colour(d.forward_v), 0);

    fmt_a(d.start_batt_a, buf, sizeof(buf));
    lv_label_set_text(s_start.a_lbl, buf);
    lv_obj_set_style_text_color(s_start.a_lbl, current_colour(d.start_batt_a), 0);

    fmt_soc(d.start_batt_soc, buf, sizeof(buf));
    lv_label_set_text(s_start.soc_lbl, buf);
    lv_obj_set_style_text_color(s_start.soc_lbl, soc_colour(d.start_batt_soc), 0);
    

    fmt_a(d.forward_a, buf, sizeof(buf));
    lv_label_set_text(s_fwd.a_lbl, buf);
    lv_obj_set_style_text_color(s_fwd.a_lbl, current_colour(d.forward_a), 0);

    fmt_soc(d.forward_soc, buf, sizeof(buf));
    lv_label_set_text(s_fwd.soc_lbl, buf);
    lv_obj_set_style_text_color(s_fwd.soc_lbl, soc_colour(d.forward_soc), 0);
}

static struct ElecReg {
    ElecReg() { ui_register_page({ LV_SYMBOL_CHARGE " ELEC", build, update }); }
} _reg;