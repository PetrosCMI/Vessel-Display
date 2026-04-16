// page_nav.cpp ΓÇö Navigation page
#include "page_registry.h"
#include "ui.h"
#include "boat_data.h"
#include "units.h"
#include "lvgl.h"
#include <math.h>

// -- Layout ----------------------------------------------------
//
//  Row 0 (h=126): [ SOG        ] [ STW        ]
//  Row 1 (h=126): [ HDG        ] [ COG        ]
//  Row 2 (h=110): [ LAT        ] [ LON        ]

static InstrCard nav_sog, nav_stw, nav_hdg, nav_cog, nav_lat, nav_lon;

static void build(lv_obj_t* tab) {
    nav_sog = make_instr_card(tab, "SOG", "kts", true);
    lv_obj_set_size(nav_sog.card, 224, 126);
    lv_obj_set_pos(nav_sog.card, 0, 0);

    nav_stw = make_instr_card(tab, "STW", "kts", true);
    lv_obj_set_size(nav_stw.card, 224, 126);
    lv_obj_set_pos(nav_stw.card, 230, 0);

    nav_hdg = make_instr_card(tab, "HDG", "\xc2\xb0""T", true);
    lv_obj_set_size(nav_hdg.card, 224, 126);
    lv_obj_set_pos(nav_hdg.card, 0, 132);

    nav_cog = make_instr_card(tab, "COG", "\xc2\xb0""T", true);
    lv_obj_set_size(nav_cog.card, 224, 126);
    lv_obj_set_pos(nav_cog.card, 230, 132);

    nav_lat = make_instr_card(tab, "LAT", "", false);
    lv_obj_set_size(nav_lat.card, 224, 106);
    lv_obj_set_pos(nav_lat.card, 0, 264);

    nav_lon = make_instr_card(tab, "LON", "", false);
    lv_obj_set_size(nav_lon.card, 224, 106);
    lv_obj_set_pos(nav_lon.card, 230, 264);
}

static void fmt_coord(float v, bool is_lat, char* buf, size_t len) {
    if (isnan(v)) { snprintf(buf, len, "---"); return; }
    int   deg = (int)fabsf(v);
    float mn  = (fabsf(v) - deg) * 60.0f;
    char  hem = is_lat ? (v >= 0 ? 'N' : 'S') : (v >= 0 ? 'E' : 'W');
    snprintf(buf, len, "%d\xc2\xb0%.3f'%c", deg, mn, hem);
}

static void update(void) {
    BoatData d = boatDataSnapshot();
    char buf[32];

    const char* su = fmt_speed(d.sog_ms, buf, sizeof(buf));
    instr_card_set(nav_sog, buf, su);

    su = fmt_speed(d.stw_ms, buf, sizeof(buf));
    instr_card_set(nav_stw, buf, su);

    if (!isnan(d.heading_deg)) {
        snprintf(buf, sizeof(buf), "%.0f", d.heading_deg);
        instr_card_set(nav_hdg, buf, "\xc2\xb0""T");
    } else {
        instr_card_set(nav_hdg, "---", "\xc2\xb0""T");
    }

    if (!isnan(d.cog_deg)) {
        snprintf(buf, sizeof(buf), "%.0f", d.cog_deg);
        instr_card_set(nav_cog, buf, "\xc2\xb0""T");
    } else {
        instr_card_set(nav_cog, "---", "\xc2\xb0""T");
    }

    fmt_coord(d.lat, true,  buf, sizeof(buf));
    instr_card_set(nav_lat, buf, d.gps_valid ? "" : "no fix");

    fmt_coord(d.lon, false, buf, sizeof(buf));
    instr_card_set(nav_lon, buf, "");
}

static struct NavReg {
    NavReg() { ui_register_page({ LV_SYMBOL_GPS " NAV", build, update }); }
} _reg;
