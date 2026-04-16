// ui_helpers.cpp — shared widget helpers used by all page files
#include "ui.h"

InstrCard make_instr_card(lv_obj_t* parent,
                           const char* name,
                           const char* unit,
                           bool large) {
    InstrCard ic;
    ic.card = lv_obj_create(parent);
    lv_obj_add_style(ic.card, &g_style_card, 0);
    lv_obj_set_scrollbar_mode(ic.card, LV_SCROLLBAR_MODE_OFF);

    ic.name_lbl = lv_label_create(ic.card);
    lv_label_set_text(ic.name_lbl, name);
    lv_obj_add_style(ic.name_lbl, &g_style_label, 0);
    lv_obj_align(ic.name_lbl, LV_ALIGN_TOP_LEFT, 0, 0);

    ic.val_lbl = lv_label_create(ic.card);
    lv_label_set_text(ic.val_lbl, "---");
    lv_obj_add_style(ic.val_lbl,
        large ? &g_style_val_large : &g_style_val_medium, 0);
    lv_obj_align(ic.val_lbl, LV_ALIGN_CENTER, 0, 8);

    ic.unit_lbl = lv_label_create(ic.card);
    lv_label_set_text(ic.unit_lbl, unit);
    lv_obj_add_style(ic.unit_lbl, &g_style_unit, 0);
    lv_obj_align(ic.unit_lbl, LV_ALIGN_BOTTOM_RIGHT, 0, 0);

    return ic;
}

void instr_card_set(InstrCard& ic,
                    const char* val_str,
                    const char* unit_str,
                    bool warn,
                    bool alert) {
    lv_label_set_text(ic.val_lbl, val_str);
    lv_label_set_text(ic.unit_lbl, unit_str);

    lv_color_t col = C_TEXT_PRI;
    if (alert) col = C_RED;
    else if (warn) col = C_YELLOW;
    lv_obj_set_style_text_color(ic.val_lbl, col, 0);
}
