#pragma once
#include <lvgl.h>

// Initialise the LVGL UI (call after lv_init and display driver setup)
void ui_init();

// Refresh all gauge values from gBoat (call every ~250 ms from main loop)
void ui_update();

// ─────────────────────────────────────────────────────────────
//  SHARED STYLES & COLOURS  (defined in ui.cpp, used by pages)
// ─────────────────────────────────────────────────────────────
extern lv_style_t g_style_screen;
extern lv_style_t g_style_card;
extern lv_style_t g_style_val_large;
extern lv_style_t g_style_val_medium;
extern lv_style_t g_style_label;
extern lv_style_t g_style_unit;

extern const lv_color_t C_BG;
extern const lv_color_t C_CARD;
extern const lv_color_t C_BORDER;
extern const lv_color_t C_ACCENT;
extern const lv_color_t C_GREEN;
extern const lv_color_t C_YELLOW;
extern const lv_color_t C_RED;
extern const lv_color_t C_TEXT_PRI;
extern const lv_color_t C_TEXT_SEC;
extern const lv_color_t C_WIND;
extern const lv_color_t C_DEPTH;

// ─────────────────────────────────────────────────────────────
//  INSTRUMENT CARD HELPER  (shared across page files)
// ─────────────────────────────────────────────────────────────
struct InstrCard {
    lv_obj_t* card;
    lv_obj_t* val_lbl;
    lv_obj_t* unit_lbl;
    lv_obj_t* name_lbl;
};

InstrCard make_instr_card(lv_obj_t* parent,
                           const char* name,
                           const char* unit,
                           bool large = true);

// Update value + unit label; colour-codes if outside warn range (NAN = ignore)
void instr_card_set(InstrCard& ic,
                    const char* val_str,
                    const char* unit_str,
                    bool warn  = false,
                    bool alert = false);
