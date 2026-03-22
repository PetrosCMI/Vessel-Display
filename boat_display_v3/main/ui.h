#pragma once
#include "lvgl.h"

void ui_init(void);
void ui_update(void);

// Navigate to a page by tab label (e.g. "DEPTH", "WIND")
// Safe to call from any task — uses lv_async_call internally
void ui_navigate_to_page(const char* page_label);

// Wake display and navigate — call from alarm_tick()
void ui_alarm_wake(const char* page_label);

// ── Shared styles (defined in ui.cpp) ────────────────────────
extern lv_style_t g_style_screen;
extern lv_style_t g_style_card;
extern lv_style_t g_style_val_large;
extern lv_style_t g_style_val_medium;
extern lv_style_t g_style_label;
extern lv_style_t g_style_unit;

// ── Colour palette ────────────────────────────────────────────
// LVGL 9 uses lv_color_make() — same as v8 lv_color_make() but
// lv_color_hex() is still available.
#define C_BG        lv_color_hex(0x0A0E17)
#define C_CARD      lv_color_hex(0x111827)
#define C_BORDER    lv_color_hex(0x1E3A5F)
#define C_ACCENT    lv_color_hex(0x00BFFF)
#define C_GREEN     lv_color_hex(0x00E676)
#define C_YELLOW    lv_color_hex(0xFFD600)
#define C_RED       lv_color_hex(0xFF1744)
#define C_TEXT_PRI  lv_color_hex(0xE8F0FE)
#define C_TEXT_SEC  lv_color_hex(0x607D8B)
#define C_WIND      lv_color_hex(0x40C4FF)
#define C_DEPTH     lv_color_hex(0x1DE9B6)

// ── Instrument card helper ────────────────────────────────────
struct InstrCard {
    lv_obj_t* card;
    lv_obj_t* val_lbl;
    lv_obj_t* unit_lbl;
    lv_obj_t* name_lbl;
};

InstrCard make_instr_card(lv_obj_t* parent, const char* name,
                           const char* unit, bool large = true);

void instr_card_set(InstrCard& ic, const char* val_str,
                    const char* unit_str,
                    bool warn = false, bool alert = false);

// Make obj and its direct children bubble scroll events to parent
void ui_make_scroll_transparent(lv_obj_t* obj);