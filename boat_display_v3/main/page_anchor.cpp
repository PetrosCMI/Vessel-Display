// page_anchor.cpp — Anchor watch with trail plot
#include "page_registry.h"
#include "ui.h"
#include "boat_data.h"
#include "settings.h"
#include "alarm.h"
#include "units.h"
#include "numpad.h"
#include "lvgl.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include <math.h>
#include <stdio.h>

// ── Trail ring buffer ─────────────────────────────────────────
#define TRAIL_MAX   120

struct TrailPoint { double lat; double lon; };
static TrailPoint  s_trail[TRAIL_MAX] = {};
static int         s_trail_head  = 0;
static int         s_trail_count = 0;
static double      s_last_trail_lat = NAN;
static double      s_last_trail_lon = NAN;

// ── Canvas ────────────────────────────────────────────────────
#define CANVAS_W   454
#define CANVAS_H   280

static lv_obj_t*   s_canvas      = NULL;
static uint8_t*    s_canvas_buf  = NULL;

// ── Widgets ───────────────────────────────────────────────────
static lv_obj_t*   s_drop_btn    = NULL;
static lv_obj_t*   s_drop_lbl    = NULL;
static lv_obj_t*   s_radius_lbl  = NULL;
static lv_obj_t*   s_dist_lbl    = NULL;
static lv_obj_t*   s_depth_lbl   = NULL;
static lv_obj_t*   s_status_lbl  = NULL;

// ── Haversine distance (metres) ───────────────────────────────
static double haversine_m(double lat1, double lon1, double lat2, double lon2) {
    const double R = 6371000.0;
    double dlat = (lat2 - lat1) * M_PI / 180.0;
    double dlon = (lon2 - lon1) * M_PI / 180.0;
    double a = sin(dlat/2)*sin(dlat/2) +
              cos(lat1*M_PI/180.0) * cos(lat2*M_PI/180.0) *
              sin(dlon/2)*sin(dlon/2);
    return R * 2.0 * atan2(sqrt(a), sqrt(1.0-a));
}

static double bearing_deg(double lat1, double lon1, double lat2, double lon2) {
    double dlon = (lon2 - lon1) * M_PI / 180.0;
    double y = sin(dlon) * cos(lat2*M_PI/180.0);
    double x = cos(lat1*M_PI/180.0)*sin(lat2*M_PI/180.0) -
              sin(lat1*M_PI/180.0)*cos(lat2*M_PI/180.0)*cos(dlon);
    double b = atan2(y, x) * 180.0 / M_PI;
    return fmod(b + 360.0, 360.0);
}

// ── Draw canvas using LVGL 9 layer API ───────────────────────
static void draw_canvas(void) {
    if (!s_canvas || !s_canvas_buf) return;

    BoatData d = boatDataSnapshot();

    lv_canvas_fill_bg(s_canvas, lv_color_hex(0x080C14), LV_OPA_COVER);

    lv_layer_t layer;
    lv_canvas_init_layer(s_canvas, &layer);

    if (!d.anchor_set || isnan(d.anchor_lat)) {
        // No anchor — show hint text
        lv_draw_label_dsc_t ldsc;
        lv_draw_label_dsc_init(&ldsc);
        ldsc.color = lv_color_hex(0x404060);
        ldsc.font  = &lv_font_montserrat_16;
        ldsc.text  = "Tap DROP to set anchor";
        ldsc.align = LV_TEXT_ALIGN_CENTER;
        lv_area_t la = {0, CANVAS_H/2 - 12, CANVAS_W - 1, CANVAS_H/2 + 12};
        lv_draw_label(&layer, &ldsc, &la);
        lv_canvas_finish_layer(s_canvas, &layer);
        return;
    }

    float radius = gSettings.anchor_radius_m;
    float scale  = (CANVAS_H / 2.0f) / (radius * 2.5f);
    int   cx     = CANVAS_W / 2;
    int   cy     = CANVAS_H / 2;
    int   r_px   = (int)(radius * scale);

    // Alarm radius circle
    lv_draw_arc_dsc_t adsc;
    lv_draw_arc_dsc_init(&adsc);
    adsc.color       = lv_color_hex(0x803020);
    adsc.width       = 2;
    adsc.center.x    = cx;
    adsc.center.y    = cy;
    adsc.radius      = (uint16_t)r_px;
    adsc.start_angle = 0;
    adsc.end_angle   = 360;
    lv_draw_arc(&layer, &adsc);

    // Guide ring at 50%
    adsc.color  = lv_color_hex(0x203040);
    adsc.width  = 1;
    adsc.radius = (uint16_t)(r_px / 2);
    lv_draw_arc(&layer, &adsc);

    // Crosshair at anchor
    lv_draw_line_dsc_t ldsc;
    lv_draw_line_dsc_init(&ldsc);
    ldsc.color = lv_color_hex(0x506080);
    ldsc.width = 1;
    ldsc.p1.x = cx - 8; ldsc.p1.y = cy;
    ldsc.p2.x = cx + 8; ldsc.p2.y = cy;
    lv_draw_line(&layer, &ldsc);
    ldsc.p1.x = cx; ldsc.p1.y = cy - 8;
    ldsc.p2.x = cx; ldsc.p2.y = cy + 8;
    lv_draw_line(&layer, &ldsc);

    // Trail lines
    if (s_trail_count > 1) {
        lv_draw_line_dsc_t tldsc;
        lv_draw_line_dsc_init(&tldsc);
        tldsc.width = 2;

        for (int i = 0; i < s_trail_count - 1; i++) {
            int idx1 = (s_trail_head - s_trail_count + i     + TRAIL_MAX) % TRAIL_MAX;
            int idx2 = (s_trail_head - s_trail_count + i + 1 + TRAIL_MAX) % TRAIL_MAX;

            double dlat1 = (s_trail[idx1].lat - d.anchor_lat) * 111320.0f;
            double dlon1 = (s_trail[idx1].lon - d.anchor_lon) *
                           111320.0f * cosf(d.anchor_lat*M_PI/180.0f);
            double dlat2 = (s_trail[idx2].lat - d.anchor_lat) * 111320.0f;
            double dlon2 = (s_trail[idx2].lon - d.anchor_lon) *
                           111320.0f * cosf(d.anchor_lat*M_PI/180.0f);

            uint8_t bright = 60 + (uint8_t)(180 * i / s_trail_count);
            tldsc.color  = lv_color_make(bright/4, bright/4, bright);
            tldsc.p1.x   = cx + (int32_t)(dlon1 * scale);
            tldsc.p1.y   = cy - (int32_t)(dlat1 * scale);
            tldsc.p2.x   = cx + (int32_t)(dlon2 * scale);
            tldsc.p2.y   = cy - (int32_t)(dlat2 * scale);
            lv_draw_line(&layer, &tldsc);
        }
    }

    // Boat position dot
    if (!isnan(d.lat) && !isnan(d.lon)) {
        double dlat = (d.lat - d.anchor_lat) * 111320.0f;
        double dlon = (d.lon - d.anchor_lon) *
                      111320.0f * cosf(d.anchor_lat*M_PI/180.0f);
        int bx = cx + (int)(dlon * scale);
        int by = cy - (int)(dlat * scale);

        bool dragging = !isnan(d.anchor_dist_m) &&
                        d.anchor_dist_m > gSettings.anchor_radius_m;

        lv_draw_arc_dsc_t bdsc;
        lv_draw_arc_dsc_init(&bdsc);
        bdsc.color       = dragging ? lv_color_hex(0xFF2020) : lv_color_hex(0x20FF40);
        bdsc.width       = 7;
        bdsc.center.x    = (int32_t)bx;
        bdsc.center.y    = (int32_t)by;
        bdsc.radius      = 5;
        bdsc.start_angle = 0;
        bdsc.end_angle   = 360;
        lv_draw_arc(&layer, &bdsc);
    }

    lv_canvas_finish_layer(s_canvas, &layer);
}

// ── Build ─────────────────────────────────────────────────────
static void build(lv_obj_t* tab) {
    // Top bar
    lv_obj_t* top = lv_obj_create(tab);
    lv_obj_add_style(top, &g_style_card, 0);
    lv_obj_set_size(top, 454, 68);
    lv_obj_set_pos(top, 0, 0);
    lv_obj_set_scrollbar_mode(top, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(top, LV_OBJ_FLAG_SCROLLABLE);

    // DROP button
    s_drop_btn = lv_btn_create(top);
    lv_obj_set_size(s_drop_btn, 100, 38);
    lv_obj_set_pos(s_drop_btn, 0, 6);
    lv_obj_set_style_bg_color(s_drop_btn, lv_color_hex(0x1A3A1A), 0);
    lv_obj_set_style_radius(s_drop_btn, 6, 0);
    s_drop_lbl = lv_label_create(s_drop_btn);
    lv_label_set_text(s_drop_lbl, LV_SYMBOL_GPS "  DROP");
    lv_obj_set_style_text_font(s_drop_lbl, &lv_font_montserrat_14, 0);
    lv_obj_center(s_drop_lbl);
    lv_obj_add_event_cb(s_drop_btn, [](lv_event_t*) {
        // BoatData d = boatDataSnapshot();
        if (xSemaphoreTake(gBoatMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
            if (gBoat.anchor_set) {
                gBoat.anchor_set         = false;
                gBoat.anchor_lat         = NAN;
                gBoat.anchor_lon         = NAN;
                gBoat.anchor_dist_m      = NAN;
                gBoat.anchor_bearing_deg = NAN;
                xSemaphoreGive(gBoatMutex);
                // Disable anchor alarm when cleared
                gSettings.alarm_enabled[ALARM_ANCHOR_DRAG] = false;
                settings_save();
            } else if (gBoat.gps_valid && !isnan(gBoat.lat)) {
                gBoat.anchor_lat         = gBoat.lat;
                gBoat.anchor_lon         = gBoat.lon;
                gBoat.anchor_set         = true;
                gBoat.anchor_dist_m      = 0;
                gBoat.anchor_bearing_deg = 0;
                xSemaphoreGive(gBoatMutex);
                // Enable anchor alarm when set
                gSettings.alarm_enabled[ALARM_ANCHOR_DRAG] = true;
                settings_save();
            } else {
                xSemaphoreGive(gBoatMutex);
            }
        }
        s_trail_head     = 0;
        s_trail_count    = 0;
        s_last_trail_lat = NAN;
        s_last_trail_lon = NAN;
    }, LV_EVENT_CLICKED, NULL);

    // Radius label
    lv_obj_t* rl = lv_label_create(top);
    lv_label_set_text(rl, "R:");
    lv_obj_add_style(rl, &g_style_label, 0);
    lv_obj_set_pos(rl, 110, 16);

    // Radius button
    lv_obj_t* rbtn = lv_btn_create(top);
    lv_obj_set_size(rbtn, 72, 36);
    lv_obj_set_pos(rbtn, 126, 8);
    lv_obj_set_style_bg_color(rbtn, lv_color_hex(0x1A2538), 0);
    lv_obj_set_style_border_color(rbtn, C_BORDER, 0);
    lv_obj_set_style_border_width(rbtn, 1, 0);
    lv_obj_set_style_radius(rbtn, 6, 0);
    s_radius_lbl = lv_label_create(rbtn);
    char rbuf[16];
    snprintf(rbuf, sizeof(rbuf), "%dm", (int)gSettings.anchor_radius_m);
    lv_label_set_text(s_radius_lbl, rbuf);
    lv_obj_set_style_text_font(s_radius_lbl, &lv_font_montserrat_14, 0);
    lv_obj_center(s_radius_lbl);
    lv_obj_add_event_cb(rbtn, [](lv_event_t*) {
        numpad_open("Alarm radius (m)",
                    gSettings.anchor_radius_m, 0,
                    10.0f, 500.0f,
                    [](float v, void*) {
                        if (isnan(v)) return;
                        gSettings.anchor_radius_m = v;
                        settings_save();
                        if (s_radius_lbl) {
                            char buf[16];
                            snprintf(buf, sizeof(buf), "%dm", (int)v);
                            lv_label_set_text(s_radius_lbl, buf);
                        }
                    }, NULL);
    }, LV_EVENT_CLICKED, NULL);

    // Drift label
    s_dist_lbl = lv_label_create(top);
    lv_label_set_text(s_dist_lbl, "---");
    lv_obj_set_style_text_font(s_dist_lbl, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(s_dist_lbl, C_TEXT_PRI, 0);
    lv_obj_set_pos(s_dist_lbl, 215, 8);
    lv_obj_t* dl = lv_label_create(top);
    lv_label_set_text(dl, "drift");
    lv_obj_add_style(dl, &g_style_label, 0);
    lv_obj_set_pos(dl, 215, 28);

    // Depth label
    s_depth_lbl = lv_label_create(top);
    lv_label_set_text(s_depth_lbl, "---");
    lv_obj_set_style_text_font(s_depth_lbl, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(s_depth_lbl, C_DEPTH, 0);
    lv_obj_set_pos(s_depth_lbl, 350, 8);
    lv_obj_t* dpl = lv_label_create(top);
    lv_label_set_text(dpl, "depth");
    lv_obj_add_style(dpl, &g_style_label, 0);
    lv_obj_set_pos(dpl, 350, 28);

    // Canvas — buffer from PSRAM
    size_t buf_size = (size_t)CANVAS_W * CANVAS_H * 2;  // RGB565 = 2 bytes/px
    s_canvas_buf = (uint8_t*)heap_caps_malloc(buf_size,
                   MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

    s_canvas = lv_canvas_create(tab);
    if (s_canvas_buf) {
        lv_canvas_set_buffer(s_canvas, s_canvas_buf,
                             CANVAS_W, CANVAS_H, LV_COLOR_FORMAT_RGB565);
    }
    lv_obj_set_pos(s_canvas, 0, 76);
    lv_canvas_fill_bg(s_canvas, lv_color_hex(0x080C14), LV_OPA_COVER);

    // Status bar
    s_status_lbl = lv_label_create(tab);
    lv_label_set_text(s_status_lbl, "No GPS fix");
    lv_obj_add_style(s_status_lbl, &g_style_label, 0);
    lv_obj_set_pos(s_status_lbl, 0, 348);
}

// ── Update ────────────────────────────────────────────────────
static void update(void) {
    BoatData d = boatDataSnapshot();
    char buf[32];

    // DROP button appearance
    if (d.anchor_set) {
        lv_label_set_text(s_drop_lbl, LV_SYMBOL_CLOSE "  CLEAR");
        lv_obj_set_style_bg_color(s_drop_btn, lv_color_hex(0x3A1A1A), 0);
    } else {
        lv_label_set_text(s_drop_lbl, LV_SYMBOL_GPS "  DROP");
        lv_obj_set_style_bg_color(s_drop_btn, lv_color_hex(0x1A3A1A), 0);
    }

    // Depth
    fmt_depth(d.depth_m, buf, sizeof(buf));
    lv_label_set_text(s_depth_lbl, buf);

    if (d.anchor_set && d.gps_valid && !isnan(d.lat)) {
        float dist = haversine_m(d.anchor_lat, d.anchor_lon, d.lat, d.lon);
        float bear = bearing_deg(d.anchor_lat, d.anchor_lon, d.lat, d.lon);

        ESP_LOGI("Anchor", "Anchor: %.6f,%.6f  Current: %.6f,%.6f  Dist: %.2fm",
            d.anchor_lat, d.anchor_lon, d.lat, d.lon, dist);

        if (xSemaphoreTake(gBoatMutex, pdMS_TO_TICKS(5)) == pdTRUE) {
            gBoat.anchor_dist_m      = dist;
            gBoat.anchor_bearing_deg = bear;
            xSemaphoreGive(gBoatMutex);
        }

        // Add trail point if moved > 2m
        bool add = isnan(s_last_trail_lat) ||
                   haversine_m(s_last_trail_lat, s_last_trail_lon,
                               d.lat, d.lon) > 2.0f;
        if (add) {
            s_trail[s_trail_head] = { d.lat, d.lon };
            s_trail_head = (s_trail_head + 1) % TRAIL_MAX;
            if (s_trail_count < TRAIL_MAX) s_trail_count++;
            s_last_trail_lat = d.lat;
            s_last_trail_lon = d.lon;
        }

        // Drift label
        if (dist < 1000.0f) snprintf(buf, sizeof(buf), "%.0fm",   dist);
        else                 snprintf(buf, sizeof(buf), "%.2fkm", dist/1000.0f);
        bool dragging = dist > gSettings.anchor_radius_m;
        lv_label_set_text(s_dist_lbl, buf);
        lv_obj_set_style_text_color(s_dist_lbl, dragging ? C_RED : C_TEXT_PRI, 0);

        snprintf(buf, sizeof(buf), "Bearing %.0f\xc2\xb0T   Max drift %.0fm", bear, dist);
        lv_label_set_text(s_status_lbl, buf);

    } else if (!d.gps_valid) {
        lv_label_set_text(s_dist_lbl, "---");
        lv_label_set_text(s_status_lbl, "No GPS fix");
    } else {
        lv_label_set_text(s_dist_lbl, "---");
        lv_label_set_text(s_status_lbl, "Tap DROP to set anchor position");
    }

    draw_canvas();
}

static struct AnchorReg {
    AnchorReg() { ui_register_page({ LV_SYMBOL_GPS " ANCH", build, update }); }
} _reg;