// page_settings.cpp — Settings page
// Units, alarm thresholds, volume, brightness — all persisted to NVS.
#include "page_registry.h"
#include "ui.h"
#include "settings.h"
#include "alarm.h"
#include "units.h"
#include "config.h"
#include <lvgl.h>

// ─────────────────────────────────────────────────────────────
//  Helpers: styled section header
// ─────────────────────────────────────────────────────────────
static lv_obj_t* make_section(lv_obj_t* parent, const char* title, int y) {
    lv_obj_t* lbl = lv_label_create(parent);
    lv_label_set_text(lbl, title);
    lv_obj_set_style_text_color(lbl, C_ACCENT, 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_16, 0);
    lv_obj_set_pos(lbl, 4, y);
    return lbl;
}

// ─────────────────────────────────────────────────────────────
//  Unit toggle button row
// ─────────────────────────────────────────────────────────────
struct UnitToggle {
    lv_obj_t*   btnmatrix;
    const char** map;
    uint8_t*    setting;   // pointer into gSettings enum field
};

static const char* speed_map[] = { "kts", "km/h", "mph", "" };
static const char* depth_map[] = { "m", "ft", "" };
static const char* temp_map[]  = { "°C", "°F", "" };
static const char* press_map[] = { "kPa", "psi", "" };

static UnitToggle unit_toggles[4];

static lv_obj_t* make_unit_row(lv_obj_t* parent,
                                const char* label,
                                const char** map,
                                uint8_t*    setting_ptr,
                                int x, int y,
                                int toggle_idx) {
    lv_obj_t* lbl = lv_label_create(parent);
    lv_label_set_text(lbl, label);
    lv_obj_add_style(lbl, &g_style_label, 0);
    lv_obj_set_pos(lbl, x, y);

    lv_obj_t* bm = lv_btnmatrix_create(parent);
    lv_btnmatrix_set_map(bm, map);
    lv_obj_set_size(bm, 200, 36);
    lv_obj_set_pos(bm, x + 80, y - 4);
    lv_obj_set_style_bg_color(bm, LV_COLOR_MAKE(0x1A, 0x25, 0x38), 0);
    lv_obj_set_style_border_color(bm, C_BORDER, 0);
    lv_btnmatrix_set_btn_ctrl(bm, *setting_ptr, LV_BTNMATRIX_CTRL_CHECKED);

    lv_obj_set_style_bg_color(bm, C_ACCENT,
        LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_text_color(bm, LV_COLOR_MAKE(0x00, 0x00, 0x00),
        LV_PART_ITEMS | LV_STATE_CHECKED);

    // Store for event handler
    unit_toggles[toggle_idx] = { bm, (const char**)map, setting_ptr };

    lv_obj_add_event_cb(bm, [](lv_event_t* e) {
        lv_obj_t* obj = lv_event_get_target(e);
        // Find which toggle this is
        for (int i = 0; i < 4; i++) {
            if (unit_toggles[i].btnmatrix == obj) {
                uint32_t idx = lv_btnmatrix_get_selected_btn(obj);
                *unit_toggles[i].setting = (uint8_t)idx;
                settings_save();
                alarm_beep(AlarmPattern::BEEP_ONCE);
                break;
            }
        }
    }, LV_EVENT_VALUE_CHANGED, NULL);

    return bm;
}

// ─────────────────────────────────────────────────────────────
//  Alarm row: enable switch + threshold spinbox(es)
// ─────────────────────────────────────────────────────────────
static lv_obj_t* alarm_switches[ALARM_COUNT];
static lv_obj_t* alarm_spinbox_lo[ALARM_COUNT];
static lv_obj_t* alarm_spinbox_hi[ALARM_COUNT];
static lv_obj_t* alarm_unit_lbl[ALARM_COUNT];

// Spinbox step sizes
static const float ALARM_STEP[ALARM_COUNT] = { 0.5f, 0.5f, 1.0f };

static void alarm_spinbox_update(AlarmID id) {
    // Update display values from current gSettings (in display units)
    if (alarm_spinbox_lo[id]) {
        float disp = alarm_threshold_display(id, gSettings.alarm_lo[id]);
        if (!isnan(disp))
            lv_spinbox_set_value(alarm_spinbox_lo[id], (int32_t)(disp * 10));
    }
    if (alarm_spinbox_hi[id]) {
        float disp = alarm_threshold_display(id, gSettings.alarm_hi[id]);
        if (!isnan(disp))
            lv_spinbox_set_value(alarm_spinbox_hi[id], (int32_t)(disp * 10));
    }
    if (alarm_unit_lbl[id]) {
        lv_label_set_text(alarm_unit_lbl[id], alarm_threshold_unit(id));
    }
}

struct AlarmRowCtx { AlarmID id; bool is_hi; };
static AlarmRowCtx alarm_ctx[ALARM_COUNT * 2];

static lv_obj_t* make_spinbox_with_buttons(lv_obj_t* parent,
                                             AlarmID id, bool is_hi,
                                             int x, int y, int ctx_idx) {
    alarm_ctx[ctx_idx] = { id, is_hi };

    lv_obj_t* sb = lv_spinbox_create(parent);
    lv_spinbox_set_range(sb, 0, 9999);       // ×10 for 1 decimal
    lv_spinbox_set_digit_format(sb, 4, 1);   // 4 digits, 1 decimal
    lv_spinbox_set_step(sb, 5);              // 0.5 steps
    lv_obj_set_size(sb, 110, 36);
    lv_obj_set_pos(sb, x, y);
    lv_obj_set_style_bg_color(sb, LV_COLOR_MAKE(0x1A, 0x25, 0x38), 0);
    lv_obj_set_style_text_color(sb, C_TEXT_PRI, 0);

    // + button
    lv_obj_t* btn_p = lv_btn_create(parent);
    lv_obj_set_size(btn_p, 30, 30);
    lv_obj_set_pos(btn_p, x + 114, y + 3);
    lv_obj_set_style_bg_color(btn_p, C_ACCENT, 0);
    lv_obj_t* lp = lv_label_create(btn_p);
    lv_label_set_text(lp, LV_SYMBOL_PLUS);
    lv_obj_center(lp);
    lv_obj_set_user_data(btn_p, (void*)(intptr_t)ctx_idx);
    lv_obj_add_event_cb(btn_p, [](lv_event_t* e) {
        int ci = (int)(intptr_t)lv_obj_get_user_data(lv_event_get_target(e));
        AlarmID id = alarm_ctx[ci].id;
        bool    hi = alarm_ctx[ci].is_hi;
        lv_obj_t* sb_obj = hi ? alarm_spinbox_hi[id] : alarm_spinbox_lo[id];
        lv_spinbox_increment(sb_obj);
        float disp = lv_spinbox_get_value(sb_obj) / 10.0f;
        float si   = alarm_threshold_to_si(id, disp);
        if (hi) gSettings.alarm_hi[id] = si;
        else    gSettings.alarm_lo[id] = si;
        settings_save();
    }, LV_EVENT_CLICKED, NULL);

    // − button
    lv_obj_t* btn_m = lv_btn_create(parent);
    lv_obj_set_size(btn_m, 30, 30);
    lv_obj_set_pos(btn_m, x - 34, y + 3);
    lv_obj_set_style_bg_color(btn_m, LV_COLOR_MAKE(0x30, 0x30, 0x40), 0);
    lv_obj_t* lm = lv_label_create(btn_m);
    lv_label_set_text(lm, LV_SYMBOL_MINUS);
    lv_obj_center(lm);
    lv_obj_set_user_data(btn_m, (void*)(intptr_t)ctx_idx);
    lv_obj_add_event_cb(btn_m, [](lv_event_t* e) {
        int ci = (int)(intptr_t)lv_obj_get_user_data(lv_event_get_target(e));
        AlarmID id = alarm_ctx[ci].id;
        bool    hi = alarm_ctx[ci].is_hi;
        lv_obj_t* sb_obj = hi ? alarm_spinbox_hi[id] : alarm_spinbox_lo[id];
        lv_spinbox_decrement(sb_obj);
        float disp = lv_spinbox_get_value(sb_obj) / 10.0f;
        float si   = alarm_threshold_to_si(id, disp);
        if (hi) gSettings.alarm_hi[id] = si;
        else    gSettings.alarm_lo[id] = si;
        settings_save();
    }, LV_EVENT_CLICKED, NULL);

    return sb;
}

// ─────────────────────────────────────────────────────────────
//  Build
// ─────────────────────────────────────────────────────────────
static lv_obj_t* settings_scroll; // scrollable content container
static lv_obj_t* vol_slider;
static lv_obj_t* bright_slider;
static lv_obj_t* mute_btn;
static lv_obj_t* mute_lbl;

static void build(lv_obj_t* tab) {
    // Scrollable container for all settings
    settings_scroll = lv_obj_create(tab);
    lv_obj_set_size(settings_scroll, 466, 390);
    lv_obj_set_pos(settings_scroll, 0, 0);
    lv_obj_set_style_bg_color(settings_scroll, C_BG, 0);
    lv_obj_set_style_border_width(settings_scroll, 0, 0);
    lv_obj_set_scroll_dir(settings_scroll, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(settings_scroll, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_set_style_pad_all(settings_scroll, 4, 0);

    lv_obj_t* p = settings_scroll;
    int y = 0;

    // ── UNITS ──────────────────────────────────────────────
    make_section(p, "  UNITS", y); y += 24;
    make_unit_row(p, "Speed",  speed_map, (uint8_t*)&gSettings.speed_unit, 4, y, 0); y += 44;
    make_unit_row(p, "Depth",  depth_map, (uint8_t*)&gSettings.depth_unit, 4, y, 1); y += 44;
    make_unit_row(p, "Temp",   temp_map,  (uint8_t*)&gSettings.temp_unit,  4, y, 2); y += 44;
    make_unit_row(p, "Press",  press_map, (uint8_t*)&gSettings.press_unit, 4, y, 3); y += 52;

    // ── ALARMS ─────────────────────────────────────────────
    make_section(p, "  ALARMS", y); y += 24;

    for (int i = 0; i < ALARM_COUNT; i++) {
        AlarmID id = ALARM_TABLE[i].id;

        // Row: label + enable switch
        lv_obj_t* lbl = lv_label_create(p);
        lv_label_set_text(lbl, ALARM_TABLE[i].label);
        lv_obj_add_style(lbl, &g_style_label, 0);
        lv_obj_set_pos(lbl, 4, y);

        alarm_switches[i] = lv_switch_create(p);
        lv_obj_set_size(alarm_switches[i], 52, 26);
        lv_obj_set_pos(alarm_switches[i], 320, y - 2);
        lv_obj_set_style_bg_color(alarm_switches[i], C_ACCENT,
            LV_PART_INDICATOR | LV_STATE_CHECKED);
        if (gSettings.alarm_enabled[i]) lv_obj_add_state(alarm_switches[i], LV_STATE_CHECKED);
        lv_obj_set_user_data(alarm_switches[i], (void*)(intptr_t)i);
        lv_obj_add_event_cb(alarm_switches[i], [](lv_event_t* e) {
            int idx = (int)(intptr_t)lv_obj_get_user_data(lv_event_get_target(e));
            gSettings.alarm_enabled[idx] =
                lv_obj_has_state(lv_event_get_target(e), LV_STATE_CHECKED);
            settings_save();
        }, LV_EVENT_VALUE_CHANGED, NULL);

        y += 36;

        // Threshold spinboxes (lo and/or hi)
        bool has_lo = !isnan(ALARM_TABLE[i].default_lo);
        bool has_hi = !isnan(ALARM_TABLE[i].default_hi);

        if (has_lo) {
            lv_obj_t* lo_lbl = lv_label_create(p);
            lv_label_set_text(lo_lbl, "MIN");
            lv_obj_add_style(lo_lbl, &g_style_label, 0);
            lv_obj_set_pos(lo_lbl, 10, y + 6);

            alarm_spinbox_lo[i] = make_spinbox_with_buttons(p, id, false, 60, y, i * 2);
            y += 44;
        } else {
            alarm_spinbox_lo[i] = nullptr;
        }

        if (has_hi) {
            lv_obj_t* hi_lbl = lv_label_create(p);
            lv_label_set_text(hi_lbl, "MAX");
            lv_obj_add_style(hi_lbl, &g_style_label, 0);
            lv_obj_set_pos(hi_lbl, 10, y + 6);

            alarm_spinbox_hi[i] = make_spinbox_with_buttons(p, id, true, 60, y, i * 2 + 1);
            y += 44;
        } else {
            alarm_spinbox_hi[i] = nullptr;
        }

        // Unit label after spinbox
        alarm_unit_lbl[i] = lv_label_create(p);
        lv_label_set_text(alarm_unit_lbl[i], alarm_threshold_unit(id));
        lv_obj_set_style_text_color(alarm_unit_lbl[i], C_ACCENT, 0);
        lv_obj_set_pos(alarm_unit_lbl[i], 220, y - 40);

        alarm_spinbox_update(id);
        y += 8;
    }

    // ── AUDIO ──────────────────────────────────────────────
    make_section(p, "  AUDIO", y); y += 28;

    lv_obj_t* vol_lbl = lv_label_create(p);
    lv_label_set_text(vol_lbl, "Volume");
    lv_obj_add_style(vol_lbl, &g_style_label, 0);
    lv_obj_set_pos(vol_lbl, 4, y + 8);

    vol_slider = lv_slider_create(p);
    lv_slider_set_range(vol_slider, 0, 255);
    lv_slider_set_value(vol_slider, gSettings.alarm_volume, LV_ANIM_OFF);
    lv_obj_set_size(vol_slider, 240, 10);
    lv_obj_set_pos(vol_slider, 90, y + 12);
    lv_obj_set_style_bg_color(vol_slider, C_ACCENT, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(vol_slider, C_ACCENT, LV_PART_KNOB);
    lv_obj_add_event_cb(vol_slider, [](lv_event_t* e) {
        gSettings.alarm_volume = (uint8_t)lv_slider_get_value(lv_event_get_target(e));
        settings_save();
    }, LV_EVENT_VALUE_CHANGED, NULL);
    y += 40;

    // Mute button
    mute_btn = lv_btn_create(p);
    lv_obj_set_size(mute_btn, 160, 40);
    lv_obj_set_pos(mute_btn, 4, y);
    lv_obj_set_style_bg_color(mute_btn, LV_COLOR_MAKE(0x40, 0x10, 0x10), 0);
    mute_lbl = lv_label_create(mute_btn);
    lv_label_set_text(mute_lbl, LV_SYMBOL_MUTE " SILENCE");
    lv_obj_center(mute_lbl);
    lv_obj_add_event_cb(mute_btn, [](lv_event_t*) {
        alarm_silence();
    }, LV_EVENT_CLICKED, NULL);
    y += 52;

    // ── DISPLAY ────────────────────────────────────────────
    make_section(p, "  DISPLAY", y); y += 28;

    lv_obj_t* br_lbl = lv_label_create(p);
    lv_label_set_text(br_lbl, "Brightness");
    lv_obj_add_style(br_lbl, &g_style_label, 0);
    lv_obj_set_pos(br_lbl, 4, y + 8);

    bright_slider = lv_slider_create(p);
    lv_slider_set_range(bright_slider, 20, 255);
    lv_slider_set_value(bright_slider, gSettings.brightness, LV_ANIM_OFF);
    lv_obj_set_size(bright_slider, 240, 10);
    lv_obj_set_pos(bright_slider, 100, y + 12);
    lv_obj_set_style_bg_color(bright_slider, C_ACCENT, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(bright_slider, C_ACCENT, LV_PART_KNOB);
    lv_obj_add_event_cb(bright_slider, [](lv_event_t* e) {
        gSettings.brightness = (uint8_t)lv_slider_get_value(lv_event_get_target(e));
        ledcWrite(0, gSettings.brightness);
        settings_save();
    }, LV_EVENT_VALUE_CHANGED, NULL);

    // Set scroll content height
    lv_obj_set_height(p, y + 60);
}

// ─────────────────────────────────────────────────────────────
//  Update — refresh spinbox unit labels when units change
// ─────────────────────────────────────────────────────────────
static void update() {
    // Refresh unit label on alarm spinboxes (user may have changed units)
    for (int i = 0; i < ALARM_COUNT; i++) {
        if (alarm_unit_lbl[i]) {
            lv_label_set_text(alarm_unit_lbl[i],
                alarm_threshold_unit(ALARM_TABLE[i].id));
        }
    }
}

// ─────────────────────────────────────────────────────────────
//  Self-register
// ─────────────────────────────────────────────────────────────
static struct SettingsPageRegistrar {
    SettingsPageRegistrar() {
        ui_register_page({ LV_SYMBOL_SETTINGS " SET", build, update });
    }
} _settings_reg;
