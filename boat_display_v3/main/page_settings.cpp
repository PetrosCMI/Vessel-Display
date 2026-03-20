// page_settings.cpp — Settings page (units, alarms, audio, brightness)
#include "page_registry.h"
#include "ui.h"
#include "settings.h"
#include "alarm.h"
#include "units.h"
#include "bsp/esp32_s3_touch_lcd_4b.h"
#include "lvgl.h"

// ── Section header helper ─────────────────────────────────────
static void make_section(lv_obj_t* p, const char* title, int y) {
    lv_obj_t* lbl = lv_label_create(p);
    lv_label_set_text(lbl, title);
    lv_obj_set_style_text_color(lbl, C_ACCENT, 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_16, 0);
    lv_obj_set_pos(lbl, 4, y);
}

// ── Unit toggle button matrix ─────────────────────────────────
struct UnitToggle { lv_obj_t* bm; uint8_t* setting; };
static UnitToggle s_unit_toggles[4];

static const char* speed_map[] = { "kts", "km/h", "mph", "" };
static const char* depth_map[] = { "m",   "ft",         "" };
static const char* temp_map[]  = { "\xc2\xb0""C", "\xc2\xb0""F", "" };
static const char* press_map[] = { "kPa", "psi",        "" };

static void make_unit_row(lv_obj_t* p, const char* label,
                           const char** map, uint8_t* setting,
                           int x, int y, int idx) {
    lv_obj_t* lbl = lv_label_create(p);
    lv_label_set_text(lbl, label);
    lv_obj_add_style(lbl, &g_style_label, 0);
    lv_obj_set_pos(lbl, x, y + 8);

    lv_obj_t* bm = lv_btnmatrix_create(p);
    lv_btnmatrix_set_map(bm, map);
    lv_obj_set_size(bm, 210, 36);
    lv_obj_set_pos(bm, x + 76, y);
    lv_obj_set_style_bg_color(bm, lv_color_hex(0x1A2538), 0);
    lv_obj_set_style_border_color(bm, C_BORDER, 0);
    lv_btnmatrix_set_btn_ctrl(bm, *setting, LV_BTNMATRIX_CTRL_CHECKED);
    lv_obj_set_style_bg_color(bm, C_ACCENT,
        LV_PART_ITEMS | LV_STATE_CHECKED);
    lv_obj_set_style_text_color(bm, lv_color_hex(0x000000),
        LV_PART_ITEMS | LV_STATE_CHECKED);

    s_unit_toggles[idx] = { bm, setting };

    lv_obj_add_event_cb(bm, [](lv_event_t* e) {
        lv_obj_t* obj = lv_event_get_target(e);
        for (int i = 0; i < 4; i++) {
            if (s_unit_toggles[i].bm != obj) continue;
            *s_unit_toggles[i].setting =
                (uint8_t)lv_btnmatrix_get_selected_btn(obj);
            settings_save();
            alarm_beep(AlarmPattern::BEEP_ONCE);
            break;
        }
    }, LV_EVENT_VALUE_CHANGED, NULL);
}

// ── Alarm rows with spinbox + buttons ────────────────────────
static lv_obj_t* s_alarm_sw[ALARM_COUNT];
static lv_obj_t* s_sb_lo[ALARM_COUNT];
static lv_obj_t* s_sb_hi[ALARM_COUNT];
static lv_obj_t* s_unit_lbl[ALARM_COUNT];

struct SpinCtx { AlarmID id; bool is_hi; };
static SpinCtx s_spin_ctx[ALARM_COUNT * 2];

static lv_obj_t* make_spinbox_row(lv_obj_t* p, AlarmID id, bool is_hi,
                                   int x, int y, int ctx_idx) {
    s_spin_ctx[ctx_idx] = { id, is_hi };

    lv_obj_t* sb = lv_spinbox_create(p);
    lv_spinbox_set_range(sb, 0, 9999);
    lv_spinbox_set_digit_format(sb, 4, 1);
    lv_spinbox_set_step(sb, 5);
    lv_obj_set_size(sb, 110, 36);
    lv_obj_set_pos(sb, x, y);
    lv_obj_set_style_bg_color(sb, lv_color_hex(0x1A2538), 0);
    lv_obj_set_style_text_color(sb, C_TEXT_PRI, 0);

    // + button
    lv_obj_t* bp = lv_btn_create(p);
    lv_obj_set_size(bp, 30, 30);
    lv_obj_set_pos(bp, x + 114, y + 3);
    lv_obj_set_style_bg_color(bp, C_ACCENT, 0);
    lv_label_set_text(lv_label_create(bp), LV_SYMBOL_PLUS);
    lv_obj_center(lv_obj_get_child(bp, 0));
    lv_obj_set_user_data(bp, (void*)(intptr_t)ctx_idx);
    lv_obj_add_event_cb(bp, [](lv_event_t* e) {
        int i = (int)(intptr_t)lv_obj_get_user_data(lv_event_get_target(e));
        AlarmID id = s_spin_ctx[i].id;
        lv_obj_t* sb = s_spin_ctx[i].is_hi ? s_sb_hi[id] : s_sb_lo[id];
        lv_spinbox_increment(sb);
        float si = alarm_threshold_to_si(id, lv_spinbox_get_value(sb) / 10.0f);
        if (s_spin_ctx[i].is_hi) gSettings.alarm_hi[id] = si;
        else                      gSettings.alarm_lo[id] = si;
        settings_save();
    }, LV_EVENT_CLICKED, NULL);

    // − button
    lv_obj_t* bm = lv_btn_create(p);
    lv_obj_set_size(bm, 30, 30);
    lv_obj_set_pos(bm, x - 34, y + 3);
    lv_obj_set_style_bg_color(bm, lv_color_hex(0x303040), 0);
    lv_label_set_text(lv_label_create(bm), LV_SYMBOL_MINUS);
    lv_obj_center(lv_obj_get_child(bm, 0));
    lv_obj_set_user_data(bm, (void*)(intptr_t)ctx_idx);
    lv_obj_add_event_cb(bm, [](lv_event_t* e) {
        int i = (int)(intptr_t)lv_obj_get_user_data(lv_event_get_target(e));
        AlarmID id = s_spin_ctx[i].id;
        lv_obj_t* sb = s_spin_ctx[i].is_hi ? s_sb_hi[id] : s_sb_lo[id];
        lv_spinbox_decrement(sb);
        float si = alarm_threshold_to_si(id, lv_spinbox_get_value(sb) / 10.0f);
        if (s_spin_ctx[i].is_hi) gSettings.alarm_hi[id] = si;
        else                      gSettings.alarm_lo[id] = si;
        settings_save();
    }, LV_EVENT_CLICKED, NULL);

    return sb;
}

// ── Build ─────────────────────────────────────────────────────
static void build(lv_obj_t* tab) {
    // Scrollable container
    lv_obj_t* p = lv_obj_create(tab);
    lv_obj_set_size(p, 466, 390);
    lv_obj_set_pos(p, 0, 0);
    lv_obj_set_style_bg_color(p, C_BG, 0);
    lv_obj_set_style_border_width(p, 0, 0);
    lv_obj_set_scroll_dir(p, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(p, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_set_style_pad_all(p, 4, 0);

    int y = 0;

    // ── UNITS ────────────────────────────────────────────────
    make_section(p, "  UNITS", y); y += 26;
    make_unit_row(p, "Speed", speed_map, (uint8_t*)&gSettings.speed_unit, 4, y, 0); y += 44;
    make_unit_row(p, "Depth", depth_map, (uint8_t*)&gSettings.depth_unit, 4, y, 1); y += 44;
    make_unit_row(p, "Temp",  temp_map,  (uint8_t*)&gSettings.temp_unit,  4, y, 2); y += 44;
    make_unit_row(p, "Press", press_map, (uint8_t*)&gSettings.press_unit, 4, y, 3); y += 52;

    // ── ALARMS ───────────────────────────────────────────────
    make_section(p, "  ALARMS", y); y += 26;

    for (int i = 0; i < ALARM_COUNT; i++) {
        AlarmID id = ALARM_TABLE[i].id;

        lv_obj_t* name = lv_label_create(p);
        lv_label_set_text(name, ALARM_TABLE[i].label);
        lv_obj_add_style(name, &g_style_label, 0);
        lv_obj_set_pos(name, 4, y);

        s_alarm_sw[i] = lv_switch_create(p);
        lv_obj_set_size(s_alarm_sw[i], 52, 26);
        lv_obj_set_pos(s_alarm_sw[i], 320, y - 2);
        lv_obj_set_style_bg_color(s_alarm_sw[i], C_ACCENT,
            LV_PART_INDICATOR | LV_STATE_CHECKED);
        if (gSettings.alarm_enabled[i])
            lv_obj_add_state(s_alarm_sw[i], LV_STATE_CHECKED);
        lv_obj_set_user_data(s_alarm_sw[i], (void*)(intptr_t)i);
        lv_obj_add_event_cb(s_alarm_sw[i], [](lv_event_t* e) {
            int idx = (int)(intptr_t)lv_obj_get_user_data(lv_event_get_target(e));
            gSettings.alarm_enabled[idx] =
                lv_obj_has_state(lv_event_get_target(e), LV_STATE_CHECKED);
            settings_save();
        }, LV_EVENT_VALUE_CHANGED, NULL);
        y += 36;

        bool has_lo = !isnan(ALARM_TABLE[i].default_lo);
        bool has_hi = !isnan(ALARM_TABLE[i].default_hi);

        if (has_lo) {
            lv_obj_t* ll = lv_label_create(p);
            lv_label_set_text(ll, "MIN");
            lv_obj_add_style(ll, &g_style_label, 0);
            lv_obj_set_pos(ll, 10, y + 6);
            s_sb_lo[i] = make_spinbox_row(p, id, false, 60, y, i * 2);
            float dv = alarm_threshold_display(id, gSettings.alarm_lo[id]);
            if (!isnan(dv)) lv_spinbox_set_value(s_sb_lo[i], (int32_t)(dv * 10));
            y += 44;
        } else { s_sb_lo[i] = nullptr; }

        if (has_hi) {
            lv_obj_t* hl = lv_label_create(p);
            lv_label_set_text(hl, "MAX");
            lv_obj_add_style(hl, &g_style_label, 0);
            lv_obj_set_pos(hl, 10, y + 6);
            s_sb_hi[i] = make_spinbox_row(p, id, true, 60, y, i * 2 + 1);
            float dv = alarm_threshold_display(id, gSettings.alarm_hi[id]);
            if (!isnan(dv)) lv_spinbox_set_value(s_sb_hi[i], (int32_t)(dv * 10));
            y += 44;
        } else { s_sb_hi[i] = nullptr; }

        s_unit_lbl[i] = lv_label_create(p);
        lv_label_set_text(s_unit_lbl[i], alarm_threshold_unit(id));
        lv_obj_set_style_text_color(s_unit_lbl[i], C_ACCENT, 0);
        lv_obj_set_pos(s_unit_lbl[i], 220, y - 40);
        y += 8;
    }

    // ── AUDIO ────────────────────────────────────────────────
    make_section(p, "  AUDIO", y); y += 28;

    lv_obj_t* vl = lv_label_create(p);
    lv_label_set_text(vl, "Volume");
    lv_obj_add_style(vl, &g_style_label, 0);
    lv_obj_set_pos(vl, 4, y + 8);

    lv_obj_t* vol_sl = lv_slider_create(p);
    lv_slider_set_range(vol_sl, 0, 100);
    lv_slider_set_value(vol_sl, gSettings.alarm_volume, LV_ANIM_OFF);
    lv_obj_set_size(vol_sl, 240, 10);
    lv_obj_set_pos(vol_sl, 80, y + 12);
    lv_obj_set_style_bg_color(vol_sl, C_ACCENT, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(vol_sl, C_ACCENT, LV_PART_KNOB);
    lv_obj_add_event_cb(vol_sl, [](lv_event_t* e) {
        gSettings.alarm_volume = (uint8_t)lv_slider_get_value(lv_event_get_target(e));
        settings_save();
    }, LV_EVENT_VALUE_CHANGED, NULL);
    y += 40;

    lv_obj_t* mute = lv_btn_create(p);
    lv_obj_set_size(mute, 160, 40);
    lv_obj_set_pos(mute, 4, y);
    lv_obj_set_style_bg_color(mute, lv_color_hex(0x401010), 0);
    lv_obj_t* ml = lv_label_create(mute);
    lv_label_set_text(ml, LV_SYMBOL_MUTE " SILENCE");
    lv_obj_center(ml);
    lv_obj_add_event_cb(mute, [](lv_event_t*) {
        alarm_silence();
    }, LV_EVENT_CLICKED, NULL);
    y += 52;

    // ── DISPLAY ──────────────────────────────────────────────
    make_section(p, "  DISPLAY", y); y += 28;

    lv_obj_t* bl = lv_label_create(p);
    lv_label_set_text(bl, "Brightness");
    lv_obj_add_style(bl, &g_style_label, 0);
    lv_obj_set_pos(bl, 4, y + 8);

    lv_obj_t* br_sl = lv_slider_create(p);
    lv_slider_set_range(br_sl, 5, 100);
    lv_slider_set_value(br_sl, gSettings.brightness_pct, LV_ANIM_OFF);
    lv_obj_set_size(br_sl, 240, 10);
    lv_obj_set_pos(br_sl, 104, y + 12);
    lv_obj_set_style_bg_color(br_sl, C_ACCENT, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(br_sl, C_ACCENT, LV_PART_KNOB);
    lv_obj_add_event_cb(br_sl, [](lv_event_t* e) {
        gSettings.brightness_pct = (int)lv_slider_get_value(lv_event_get_target(e));
        bsp_display_brightness_set(gSettings.brightness_pct);
        settings_save();
    }, LV_EVENT_VALUE_CHANGED, NULL);

    lv_obj_set_height(p, y + 60);
}

// ── Update — refresh unit labels when units change ─────────────
static void update(void) {
    for (int i = 0; i < ALARM_COUNT; i++) {
        if (s_unit_lbl[i])
            lv_label_set_text(s_unit_lbl[i], alarm_threshold_unit(ALARM_TABLE[i].id));
    }
}

static struct SettingsReg {
    SettingsReg() { ui_register_page({ LV_SYMBOL_SETTINGS " SET", build, update }); }
} _reg;
