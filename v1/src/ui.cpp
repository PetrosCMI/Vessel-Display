#include "ui.h"
#include "boat_data.h"
#include <lvgl.h>
#include <math.h>

// ═══════════════════════════════════════════════════════════════
//  COLOUR PALETTE  (dark marine theme)
// ═══════════════════════════════════════════════════════════════
#define C_BG        lv_color_hex(0x0A0E17)   // near-black navy
#define C_CARD      lv_color_hex(0x111827)   // card background
#define C_BORDER    lv_color_hex(0x1E3A5F)   // subtle blue border
#define C_ACCENT    lv_color_hex(0x00BFFF)   // deep sky blue
#define C_GREEN     lv_color_hex(0x00E676)   // healthy / normal
#define C_YELLOW    lv_color_hex(0xFFD600)   // caution
#define C_RED       lv_color_hex(0xFF1744)   // warning
#define C_TEXT_PRI  lv_color_hex(0xE8F0FE)   // primary text
#define C_TEXT_SEC  lv_color_hex(0x607D8B)   // secondary / label
#define C_WIND      lv_color_hex(0x40C4FF)   // wind accent
#define C_DEPTH     lv_color_hex(0x1DE9B6)   // depth teal

// ═══════════════════════════════════════════════════════════════
//  SHARED STYLES
// ═══════════════════════════════════════════════════════════════
static lv_style_t style_screen;
static lv_style_t style_card;
static lv_style_t style_val_large;
static lv_style_t style_val_medium;
static lv_style_t style_label;
static lv_style_t style_unit;
static lv_style_t style_tab_btn;
static lv_style_t style_status_bar;

static void init_styles() {
    // Screen background
    lv_style_init(&style_screen);
    lv_style_set_bg_color(&style_screen, C_BG);
    lv_style_set_bg_opa(&style_screen, LV_OPA_COVER);

    // Card / tile
    lv_style_init(&style_card);
    lv_style_set_bg_color(&style_card, C_CARD);
    lv_style_set_bg_opa(&style_card, LV_OPA_COVER);
    lv_style_set_border_color(&style_card, C_BORDER);
    lv_style_set_border_width(&style_card, 1);
    lv_style_set_radius(&style_card, 12);
    lv_style_set_pad_all(&style_card, 10);

    // Large value (e.g. SOG number)
    lv_style_init(&style_val_large);
    lv_style_set_text_font(&style_val_large, &lv_font_montserrat_48);
    lv_style_set_text_color(&style_val_large, C_TEXT_PRI);

    // Medium value
    lv_style_init(&style_val_medium);
    lv_style_set_text_font(&style_val_medium, &lv_font_montserrat_32);
    lv_style_set_text_color(&style_val_medium, C_TEXT_PRI);

    // Instrument label (e.g. "SOG")
    lv_style_init(&style_label);
    lv_style_set_text_font(&style_label, &lv_font_montserrat_16);
    lv_style_set_text_color(&style_label, C_TEXT_SEC);

    // Unit label (e.g. "kts")
    lv_style_init(&style_unit);
    lv_style_set_text_font(&style_unit, &lv_font_montserrat_16);
    lv_style_set_text_color(&style_unit, C_ACCENT);

    // Status bar
    lv_style_init(&style_status_bar);
    lv_style_set_bg_color(&style_status_bar, lv_color_hex(0x060A12));
    lv_style_set_bg_opa(&style_status_bar, LV_OPA_COVER);
}

// ═══════════════════════════════════════════════════════════════
//  INSTRUMENT CARD BUILDER
//  Creates a titled card with a large value label + unit label
// ═══════════════════════════════════════════════════════════════
struct InstrCard {
    lv_obj_t* card;
    lv_obj_t* val_lbl;
    lv_obj_t* unit_lbl;
    lv_obj_t* name_lbl;
};

static InstrCard make_card(lv_obj_t* parent,
                            const char* name,
                            const char* unit,
                            bool large = true) {
    InstrCard ic;
    ic.card = lv_obj_create(parent);
    lv_obj_add_style(ic.card, &style_card, 0);
    lv_obj_set_scrollbar_mode(ic.card, LV_SCROLLBAR_MODE_OFF);

    ic.name_lbl = lv_label_create(ic.card);
    lv_label_set_text(ic.name_lbl, name);
    lv_obj_add_style(ic.name_lbl, &style_label, 0);
    lv_obj_align(ic.name_lbl, LV_ALIGN_TOP_LEFT, 0, 0);

    ic.val_lbl = lv_label_create(ic.card);
    lv_label_set_text(ic.val_lbl, "---");
    lv_obj_add_style(ic.val_lbl, large ? &style_val_large : &style_val_medium, 0);
    lv_obj_align(ic.val_lbl, LV_ALIGN_CENTER, 0, 8);

    ic.unit_lbl = lv_label_create(ic.card);
    lv_label_set_text(ic.unit_lbl, unit);
    lv_obj_add_style(ic.unit_lbl, &style_unit, 0);
    lv_obj_align(ic.unit_lbl, LV_ALIGN_BOTTOM_RIGHT, 0, 0);

    return ic;
}

// ═══════════════════════════════════════════════════════════════
//  COMPASS WIDGET  (hand-drawn arc + needle via LVGL arc+line)
// ═══════════════════════════════════════════════════════════════
struct CompassWidget {
    lv_obj_t* container;
    lv_obj_t* arc;
    lv_obj_t* hdg_label;   // numeric heading
    lv_obj_t* cog_label;   // COG value
    float     last_hdg;
};

static CompassWidget make_compass(lv_obj_t* parent, int x, int y, int size) {
    CompassWidget cw;
    cw.last_hdg = 0;

    cw.container = lv_obj_create(parent);
    lv_obj_add_style(cw.container, &style_card, 0);
    lv_obj_set_size(cw.container, size, size);
    lv_obj_set_pos(cw.container, x, y);
    lv_obj_set_scrollbar_mode(cw.container, LV_SCROLLBAR_MODE_OFF);

    // Outer ring arc (decorative)
    cw.arc = lv_arc_create(cw.container);
    lv_arc_set_range(cw.arc, 0, 360);
    lv_arc_set_value(cw.arc, 0);
    lv_arc_set_bg_angles(cw.arc, 0, 360);
    lv_obj_set_size(cw.arc, size - 30, size - 30);
    lv_obj_center(cw.arc);
    lv_obj_set_style_arc_color(cw.arc, C_BORDER, LV_PART_MAIN);
    lv_obj_set_style_arc_color(cw.arc, C_ACCENT, LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(cw.arc, 4, LV_PART_MAIN);
    lv_obj_set_style_arc_width(cw.arc, 4, LV_PART_INDICATOR);
    lv_obj_remove_style(cw.arc, NULL, LV_PART_KNOB);
    lv_obj_clear_flag(cw.arc, LV_OBJ_FLAG_CLICKABLE);

    // Heading value label (centre)
    cw.hdg_label = lv_label_create(cw.container);
    lv_label_set_text(cw.hdg_label, "---°");
    lv_obj_add_style(cw.hdg_label, &style_val_medium, 0);
    lv_obj_center(cw.hdg_label);

    // Small "HDG" title
    lv_obj_t* title = lv_label_create(cw.container);
    lv_label_set_text(title, "HDG");
    lv_obj_add_style(title, &style_label, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 4);

    // COG sub-label
    cw.cog_label = lv_label_create(cw.container);
    lv_label_set_text(cw.cog_label, "COG ---°");
    lv_obj_add_style(cw.cog_label, &style_label, 0);
    lv_obj_align(cw.cog_label, LV_ALIGN_BOTTOM_MID, 0, -4);

    return cw;
}

static void update_compass(CompassWidget& cw, float hdg, float cog) {
    if (!isnan(hdg)) {
        lv_arc_set_value(cw.arc, (int)hdg);
        char buf[16];
        snprintf(buf, sizeof(buf), "%.0f°", hdg);
        lv_label_set_text(cw.hdg_label, buf);
    }
    if (!isnan(cog)) {
        char buf[24];
        snprintf(buf, sizeof(buf), "COG %.0f°", cog);
        lv_label_set_text(cw.cog_label, buf);
    }
}

// ═══════════════════════════════════════════════════════════════
//  WIND ROSE  (simple arc indicator)
// ═══════════════════════════════════════════════════════════════
struct WindRose {
    lv_obj_t* container;
    lv_obj_t* arc;
    lv_obj_t* awa_label;
    lv_obj_t* twa_label;
};

static WindRose make_wind_rose(lv_obj_t* parent, int x, int y, int size) {
    WindRose wr;

    wr.container = lv_obj_create(parent);
    lv_obj_add_style(wr.container, &style_card, 0);
    lv_obj_set_size(wr.container, size, size);
    lv_obj_set_pos(wr.container, x, y);
    lv_obj_set_scrollbar_mode(wr.container, LV_SCROLLBAR_MODE_OFF);

    lv_obj_t* title = lv_label_create(wr.container);
    lv_label_set_text(title, "WIND ANGLE");
    lv_obj_add_style(title, &style_label, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 0);

    wr.arc = lv_arc_create(wr.container);
    lv_arc_set_range(wr.arc, -180, 180);
    lv_arc_set_value(wr.arc, 0);
    lv_arc_set_bg_angles(wr.arc, 0, 360);
    lv_obj_set_size(wr.arc, size - 36, size - 36);
    lv_obj_center(wr.arc);
    lv_obj_set_style_arc_color(wr.arc, C_BORDER, LV_PART_MAIN);
    lv_obj_set_style_arc_color(wr.arc, C_WIND, LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(wr.arc, 5, LV_PART_MAIN);
    lv_obj_set_style_arc_width(wr.arc, 5, LV_PART_INDICATOR);
    lv_obj_remove_style(wr.arc, NULL, LV_PART_KNOB);
    lv_obj_clear_flag(wr.arc, LV_OBJ_FLAG_CLICKABLE);

    wr.awa_label = lv_label_create(wr.container);
    lv_label_set_text(wr.awa_label, "AWA ---°");
    lv_obj_add_style(wr.awa_label, &style_label, 0);
    lv_obj_align(wr.awa_label, LV_ALIGN_CENTER, 0, -10);

    wr.twa_label = lv_label_create(wr.container);
    lv_label_set_text(wr.twa_label, "TWA ---°");
    lv_obj_add_style(wr.twa_label, &style_label, 0);
    lv_obj_align(wr.twa_label, LV_ALIGN_CENTER, 0, 10);

    return wr;
}

static void update_wind_rose(WindRose& wr, float awa, float twa) {
    char buf[24];
    if (!isnan(awa)) {
        lv_arc_set_value(wr.arc, (int)awa);
        snprintf(buf, sizeof(buf), "AWA %+.0f°", awa);
        lv_label_set_text(wr.awa_label, buf);
    }
    if (!isnan(twa)) {
        snprintf(buf, sizeof(buf), "TWA %+.0f°", twa);
        lv_label_set_text(wr.twa_label, buf);
    }
}

// ═══════════════════════════════════════════════════════════════
//  STATUS BAR  (top strip: connection + time since last update)
// ═══════════════════════════════════════════════════════════════
static lv_obj_t* sb_conn_dot;
static lv_obj_t* sb_conn_lbl;
static lv_obj_t* sb_age_lbl;

static void make_status_bar(lv_obj_t* parent) {
    lv_obj_t* bar = lv_obj_create(parent);
    lv_obj_set_size(bar, 480, 28);
    lv_obj_set_pos(bar, 0, 0);
    lv_obj_add_style(bar, &style_status_bar, 0);
    lv_obj_set_scrollbar_mode(bar, LV_SCROLLBAR_MODE_OFF);

    sb_conn_dot = lv_label_create(bar);
    lv_label_set_text(sb_conn_dot, LV_SYMBOL_WIFI);
    lv_obj_set_style_text_color(sb_conn_dot, C_RED, 0);
    lv_obj_align(sb_conn_dot, LV_ALIGN_LEFT_MID, 6, 0);

    sb_conn_lbl = lv_label_create(bar);
    lv_label_set_text(sb_conn_lbl, "Disconnected");
    lv_obj_set_style_text_color(sb_conn_lbl, C_TEXT_SEC, 0);
    lv_obj_set_style_text_font(sb_conn_lbl, &lv_font_montserrat_12, 0);
    lv_obj_align(sb_conn_dot, LV_ALIGN_LEFT_MID, 28, 0);
    lv_obj_align(sb_conn_lbl, LV_ALIGN_LEFT_MID, 52, 0);

    sb_age_lbl = lv_label_create(bar);
    lv_label_set_text(sb_age_lbl, "");
    lv_obj_set_style_text_color(sb_age_lbl, C_TEXT_SEC, 0);
    lv_obj_set_style_text_font(sb_age_lbl, &lv_font_montserrat_12, 0);
    lv_obj_align(sb_age_lbl, LV_ALIGN_RIGHT_MID, -8, 0);
}

static void update_status_bar(bool connected, unsigned long last_ms) {
    lv_obj_set_style_text_color(sb_conn_dot,
        connected ? C_GREEN : C_RED, 0);
    lv_label_set_text(sb_conn_lbl,
        connected ? "SignalK" : "Disconnected");

    if (connected && last_ms > 0) {
        char buf[24];
        unsigned long age = (millis() - last_ms) / 1000;
        if (age < 60)
            snprintf(buf, sizeof(buf), "%lus ago", age);
        else
            snprintf(buf, sizeof(buf), ">1min");
        lv_label_set_text(sb_age_lbl, buf);
    } else {
        lv_label_set_text(sb_age_lbl, "");
    }
}

// ═══════════════════════════════════════════════════════════════
//  PAGE WIDGETS  (declared at file scope so ui_update can reach)
// ═══════════════════════════════════════════════════════════════

// --- NAV PAGE ---
static InstrCard   nav_sog, nav_stw, nav_lat, nav_lon;
static CompassWidget nav_compass;

// --- WIND PAGE ---
static InstrCard   wind_aws, wind_tws, wind_twd;
static WindRose    wind_rose;

// --- DEPTH PAGE ---
static InstrCard   dep_depth, dep_water_temp, dep_air_temp;

// --- ENGINE PAGE ---
static InstrCard   eng_rpm, eng_coolant, eng_oil, eng_batt, eng_alt;

// ═══════════════════════════════════════════════════════════════
//  PAGE BUILDERS
// ═══════════════════════════════════════════════════════════════

// Content area starts at y=28 (below status bar), height = 480-28-50 = 402
// Tab bar is at bottom, ~50px

static void build_nav_page(lv_obj_t* tab) {
    lv_obj_set_scrollbar_mode(tab, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_bg_color(tab, C_BG, 0);
    lv_obj_set_style_pad_all(tab, 6, 0);

    // Compass: left column, tall
    nav_compass = make_compass(tab, 0, 0, 220);

    // SOG card — top right
    nav_sog = make_card(tab, "SOG", "kts", true);
    lv_obj_set_size(nav_sog.card, 226, 100);
    lv_obj_set_pos(nav_sog.card, 228, 0);

    // STW card — mid right
    nav_stw = make_card(tab, "STW", "kts", true);
    lv_obj_set_size(nav_stw.card, 226, 100);
    lv_obj_set_pos(nav_stw.card, 228, 108);

    // LAT / LON — bottom strip
    nav_lat = make_card(tab, "LAT", "°", false);
    lv_obj_set_size(nav_lat.card, 226, 100);
    lv_obj_set_pos(nav_lat.card, 0, 228);

    nav_lon = make_card(tab, "LON", "°", false);
    lv_obj_set_size(nav_lon.card, 226, 100);
    lv_obj_set_pos(nav_lon.card, 228, 228);
}

static void build_wind_page(lv_obj_t* tab) {
    lv_obj_set_scrollbar_mode(tab, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_bg_color(tab, C_BG, 0);
    lv_obj_set_style_pad_all(tab, 6, 0);

    wind_rose = make_wind_rose(tab, 0, 0, 220);

    wind_aws = make_card(tab, "AWS", "kts", true);
    lv_obj_set_size(wind_aws.card, 226, 100);
    lv_obj_set_pos(wind_aws.card, 228, 0);

    wind_tws = make_card(tab, "TWS", "kts", true);
    lv_obj_set_size(wind_tws.card, 226, 100);
    lv_obj_set_pos(wind_tws.card, 228, 108);

    wind_twd = make_card(tab, "TWD", "°T", false);
    lv_obj_set_size(wind_twd.card, 454, 100);
    lv_obj_set_pos(wind_twd.card, 0, 228);
}

static void build_depth_page(lv_obj_t* tab) {
    lv_obj_set_scrollbar_mode(tab, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_bg_color(tab, C_BG, 0);
    lv_obj_set_style_pad_all(tab, 6, 0);

    // Large depth display in centre
    dep_depth = make_card(tab, "DEPTH", "m", true);
    lv_obj_set_size(dep_depth.card, 454, 180);
    lv_obj_set_pos(dep_depth.card, 0, 0);
    lv_obj_set_style_text_font(dep_depth.val_lbl, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(dep_depth.val_lbl, C_DEPTH, 0);

    dep_water_temp = make_card(tab, "WATER TEMP", "°C", false);
    lv_obj_set_size(dep_water_temp.card, 222, 140);
    lv_obj_set_pos(dep_water_temp.card, 0, 190);

    dep_air_temp = make_card(tab, "AIR TEMP", "°C", false);
    lv_obj_set_size(dep_air_temp.card, 222, 140);
    lv_obj_set_pos(dep_air_temp.card, 232, 190);
}

static void build_engine_page(lv_obj_t* tab) {
    lv_obj_set_scrollbar_mode(tab, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_bg_color(tab, C_BG, 0);
    lv_obj_set_style_pad_all(tab, 6, 0);

    // RPM — wide top card
    eng_rpm = make_card(tab, "ENGINE RPM", "rpm", true);
    lv_obj_set_size(eng_rpm.card, 454, 120);
    lv_obj_set_pos(eng_rpm.card, 0, 0);

    eng_coolant = make_card(tab, "COOLANT", "°C", false);
    lv_obj_set_size(eng_coolant.card, 148, 110);
    lv_obj_set_pos(eng_coolant.card, 0, 130);

    eng_oil = make_card(tab, "OIL kPa", "kPa", false);
    lv_obj_set_size(eng_oil.card, 148, 110);
    lv_obj_set_pos(eng_oil.card, 156, 130);

    eng_batt = make_card(tab, "HOUSE V", "V", false);
    lv_obj_set_size(eng_batt.card, 148, 110);
    lv_obj_set_pos(eng_batt.card, 0, 250);

    eng_alt = make_card(tab, "START V", "V", false);
    lv_obj_set_size(eng_alt.card, 148, 110);
    lv_obj_set_pos(eng_alt.card, 156, 250);
}

// ═══════════════════════════════════════════════════════════════
//  PUBLIC: ui_init
// ═══════════════════════════════════════════════════════════════
void ui_init() {
    init_styles();

    lv_obj_t* scr = lv_scr_act();
    lv_obj_add_style(scr, &style_screen, 0);
    lv_obj_set_scrollbar_mode(scr, LV_SCROLLBAR_MODE_OFF);

    // Status bar (always visible, above tabview)
    make_status_bar(scr);

    // Tab view — slides horizontally, tab bar at bottom
    lv_obj_t* tv = lv_tabview_create(scr, LV_DIR_BOTTOM, 46);
    lv_obj_set_pos(tv, 0, 28);
    lv_obj_set_size(tv, 480, 452);
    lv_obj_set_style_bg_color(tv, C_BG, 0);

    // Style the tab buttons
    lv_obj_t* tab_btns = lv_tabview_get_tab_btns(tv);
    lv_obj_set_style_bg_color(tab_btns, lv_color_hex(0x060A12), 0);
    lv_obj_set_style_text_color(tab_btns, C_TEXT_SEC, 0);
    lv_obj_set_style_text_color(tab_btns, C_ACCENT, LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_border_side(tab_btns, LV_BORDER_SIDE_TOP, LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_border_color(tab_btns, C_ACCENT, LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_border_width(tab_btns, 2, LV_PART_ITEMS | LV_STATE_CHECKED);

    lv_obj_t* tab_nav    = lv_tabview_add_tab(tv, LV_SYMBOL_GPS   " NAV");
    lv_obj_t* tab_wind   = lv_tabview_add_tab(tv, LV_SYMBOL_UP    " WIND");
    lv_obj_t* tab_depth  = lv_tabview_add_tab(tv, LV_SYMBOL_DOWN  " DEPTH");
    lv_obj_t* tab_engine = lv_tabview_add_tab(tv, LV_SYMBOL_SETTINGS " ENG");

    build_nav_page(tab_nav);
    build_wind_page(tab_wind);
    build_depth_page(tab_depth);
    build_engine_page(tab_engine);
}

// ═══════════════════════════════════════════════════════════════
//  PUBLIC: ui_update  (call every 250 ms)
// ═══════════════════════════════════════════════════════════════
static void set_val_f(InstrCard& ic, float v, const char* fmt, float warn_lo = NAN, float warn_hi = NAN) {
    if (isnan(v)) {
        lv_label_set_text(ic.val_lbl, "---");
        lv_obj_set_style_text_color(ic.val_lbl, C_TEXT_PRI, 0);
        return;
    }
    char buf[24];
    snprintf(buf, sizeof(buf), fmt, v);
    lv_label_set_text(ic.val_lbl, buf);

    // Colour coding for warnings
    if (!isnan(warn_lo) && v < warn_lo)
        lv_obj_set_style_text_color(ic.val_lbl, C_YELLOW, 0);
    else if (!isnan(warn_hi) && v > warn_hi)
        lv_obj_set_style_text_color(ic.val_lbl, C_RED, 0);
    else
        lv_obj_set_style_text_color(ic.val_lbl, C_TEXT_PRI, 0);
}

static void set_coord(InstrCard& ic, float v, bool is_lat) {
    if (isnan(v)) { lv_label_set_text(ic.val_lbl, "---"); return; }
    int deg  = (int)fabs(v);
    float mn = (fabs(v) - deg) * 60.0f;
    char hem = is_lat ? (v >= 0 ? 'N' : 'S') : (v >= 0 ? 'E' : 'W');
    char buf[24];
    snprintf(buf, sizeof(buf), "%d° %.3f' %c", deg, mn, hem);
    lv_label_set_text(ic.val_lbl, buf);
}

void ui_update() {
    BoatData d = boatDataSnapshot();

    // Status bar
    update_status_bar(d.signalk_connected, d.last_update_ms);

    // ── NAV ────────────────────────────────────────
    set_val_f(nav_sog, d.sog_kts, "%.1f");
    set_val_f(nav_stw, d.stw_kts, "%.1f");
    set_coord(nav_lat, d.lat, true);
    set_coord(nav_lon, d.lon, false);
    update_compass(nav_compass, d.heading_deg, d.cog_deg);

    // ── WIND ───────────────────────────────────────
    set_val_f(wind_aws, d.aws_kts, "%.1f");
    set_val_f(wind_tws, d.tws_kts, "%.1f");
    set_val_f(wind_twd, d.twd_deg, "%.0f");
    update_wind_rose(wind_rose, d.awa_deg, d.twa_deg);

    // ── DEPTH ──────────────────────────────────────
    set_val_f(dep_depth,      d.depth_m,      "%.1f", 2.0f, NAN);  // warn < 2m
    set_val_f(dep_water_temp, d.water_temp_c, "%.1f");
    set_val_f(dep_air_temp,   d.air_temp_c,   "%.1f");

    // ── ENGINE ─────────────────────────────────────
    set_val_f(eng_rpm,     d.rpm,          "%.0f",  NAN, 3500.0f);
    set_val_f(eng_coolant, d.coolant_temp, "%.0f",  NAN,   95.0f);  // warn > 95°C
    set_val_f(eng_oil,     d.oil_pressure, "%.0f",  150.0f, NAN);   // warn < 150 kPa
    set_val_f(eng_batt,    d.battery_v,    "%.2f",  12.0f,  14.8f);
    set_val_f(eng_alt,     d.alt_v,        "%.2f",  12.0f,  14.8f);
}
