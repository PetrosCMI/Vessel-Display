// page_nav.cpp — Navigation page
// Registers itself via a static constructor; no other file needs editing.
#include "page_registry.h"
#include "ui.h"
#include "boat_data.h"
#include "units.h"
#include <lvgl.h>
#include <math.h>

// ── Widgets ──────────────────────────────────────────────────
static InstrCard nav_sog, nav_stw, nav_lat, nav_lon;
static lv_obj_t* nav_compass_arc;
static lv_obj_t* nav_hdg_lbl;
static lv_obj_t* nav_cog_lbl;

// ── Compass widget ───────────────────────────────────────────
static void build_compass(lv_obj_t* parent, int x, int y, int sz) {
    lv_obj_t* c = lv_obj_create(parent);
    lv_obj_add_style(c, &g_style_card, 0);
    lv_obj_set_size(c, sz, sz);
    lv_obj_set_pos(c, x, y);
    lv_obj_set_scrollbar_mode(c, LV_SCROLLBAR_MODE_OFF);

    lv_obj_t* title = lv_label_create(c);
    lv_label_set_text(title, "HDG");
    lv_obj_add_style(title, &g_style_label, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 2);

    nav_compass_arc = lv_arc_create(c);
    lv_arc_set_range(nav_compass_arc, 0, 359);
    lv_arc_set_value(nav_compass_arc, 0);
    lv_arc_set_bg_angles(nav_compass_arc, 0, 360);
    lv_obj_set_size(nav_compass_arc, sz - 32, sz - 32);
    lv_obj_center(nav_compass_arc);
    lv_obj_set_style_arc_color(nav_compass_arc, C_BORDER, LV_PART_MAIN);
    lv_obj_set_style_arc_color(nav_compass_arc, C_ACCENT, LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(nav_compass_arc, 5, LV_PART_MAIN);
    lv_obj_set_style_arc_width(nav_compass_arc, 5, LV_PART_INDICATOR);
    lv_obj_remove_style(nav_compass_arc, NULL, LV_PART_KNOB);
    lv_obj_clear_flag(nav_compass_arc, LV_OBJ_FLAG_CLICKABLE);

    nav_hdg_lbl = lv_label_create(c);
    lv_label_set_text(nav_hdg_lbl, "---°");
    lv_obj_add_style(nav_hdg_lbl, &g_style_val_medium, 0);
    lv_obj_align(nav_hdg_lbl, LV_ALIGN_CENTER, 0, -6);

    nav_cog_lbl = lv_label_create(c);
    lv_label_set_text(nav_cog_lbl, "COG ---°");
    lv_obj_add_style(nav_cog_lbl, &g_style_label, 0);
    lv_obj_align(nav_cog_lbl, LV_ALIGN_BOTTOM_MID, 0, -4);
}

// ── Build ────────────────────────────────────────────────────
static void build(lv_obj_t* tab) {
    build_compass(tab, 0, 0, 220);

    nav_sog = make_instr_card(tab, "SOG", "kts", true);
    lv_obj_set_size(nav_sog.card, 226, 100);
    lv_obj_set_pos(nav_sog.card, 228, 0);

    nav_stw = make_instr_card(tab, "STW", "kts", true);
    lv_obj_set_size(nav_stw.card, 226, 100);
    lv_obj_set_pos(nav_stw.card, 228, 108);

    nav_lat = make_instr_card(tab, "LAT", "°", false);
    lv_obj_set_size(nav_lat.card, 226, 100);
    lv_obj_set_pos(nav_lat.card, 0, 228);

    nav_lon = make_instr_card(tab, "LON", "°", false);
    lv_obj_set_size(nav_lon.card, 226, 100);
    lv_obj_set_pos(nav_lon.card, 228, 228);
}

// ── Update ───────────────────────────────────────────────────
static void fmt_coord(float v, bool is_lat, char* buf, size_t len) {
    if (isnan(v)) { snprintf(buf, len, "---"); return; }
    int   deg = (int)fabs(v);
    float mn  = (fabs(v) - deg) * 60.0f;
    char  hem = is_lat ? (v >= 0 ? 'N' : 'S') : (v >= 0 ? 'E' : 'W');
    snprintf(buf, len, "%d° %.3f' %c", deg, mn, hem);
}

static void update() {
    BoatData d = boatDataSnapshot();
    char buf[32];

    // SOG
    const char* su = fmt_speed(d.sog_kts / 1.94384f, buf, sizeof(buf));
    instr_card_set(nav_sog, buf, su);

    // STW
    su = fmt_speed(d.stw_kts / 1.94384f, buf, sizeof(buf));
    instr_card_set(nav_stw, buf, su);

    // Heading compass
    if (!isnan(d.heading_deg)) {
        lv_arc_set_value(nav_compass_arc, (int)d.heading_deg);
        snprintf(buf, sizeof(buf), "%.0f°", d.heading_deg);
        lv_label_set_text(nav_hdg_lbl, buf);
    }
    if (!isnan(d.cog_deg)) {
        snprintf(buf, sizeof(buf), "COG %.0f°", d.cog_deg);
        lv_label_set_text(nav_cog_lbl, buf);
    }

    // Lat / Lon
    fmt_coord(d.lat, true,  buf, sizeof(buf));
    instr_card_set(nav_lat, buf, d.gps_valid ? "" : "no fix");

    fmt_coord(d.lon, false, buf, sizeof(buf));
    instr_card_set(nav_lon, buf, d.gps_valid ? "" : "");
}

// ── Self-register ────────────────────────────────────────────
static struct NavPageRegistrar {
    NavPageRegistrar() {
        ui_register_page({ LV_SYMBOL_GPS " NAV", build, update });
    }
} _nav_registrar;
