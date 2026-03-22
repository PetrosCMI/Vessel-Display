#include "ui.h"

InstrCard make_instr_card(lv_obj_t* parent, const char* name,
                           const char* unit, bool large) {
    InstrCard ic;
    ic.card = lv_obj_create(parent);
    lv_obj_add_style(ic.card, &g_style_card, 0);
    lv_obj_set_scrollbar_mode(ic.card, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(ic.card, LV_OBJ_FLAG_SCROLLABLE);
    // Bubble scroll events up to the parent scrollable pane
    lv_obj_add_flag(ic.card, LV_OBJ_FLAG_EVENT_BUBBLE);

    ic.name_lbl = lv_label_create(ic.card);
    lv_label_set_text(ic.name_lbl, name);
    lv_obj_add_style(ic.name_lbl, &g_style_label, 0);
    lv_obj_align(ic.name_lbl, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_add_flag(ic.name_lbl, LV_OBJ_FLAG_EVENT_BUBBLE);

    ic.val_lbl = lv_label_create(ic.card);
    lv_label_set_text(ic.val_lbl, "---");
    lv_obj_add_style(ic.val_lbl,
        large ? &g_style_val_large : &g_style_val_medium, 0);
    lv_obj_align(ic.val_lbl, LV_ALIGN_CENTER, 0, 8);
    lv_obj_add_flag(ic.val_lbl, LV_OBJ_FLAG_EVENT_BUBBLE);

    ic.unit_lbl = lv_label_create(ic.card);
    lv_label_set_text(ic.unit_lbl, unit);
    lv_obj_add_style(ic.unit_lbl, &g_style_unit, 0);
    lv_obj_align(ic.unit_lbl, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
    lv_obj_add_flag(ic.unit_lbl, LV_OBJ_FLAG_EVENT_BUBBLE);

    return ic;
}

// Make any object and all its direct children bubble scroll events upward
void ui_make_scroll_transparent(lv_obj_t* obj) {
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(obj, LV_OBJ_FLAG_EVENT_BUBBLE);
    uint32_t cnt = lv_obj_get_child_count(obj);
    for (uint32_t i = 0; i < cnt; i++) {
        lv_obj_t* child = lv_obj_get_child(obj, i);
        lv_obj_clear_flag(child, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(child, LV_OBJ_FLAG_EVENT_BUBBLE);
    }
}

void instr_card_set(InstrCard& ic, const char* val_str,
                    const char* unit_str, bool warn, bool alert) {
    lv_label_set_text(ic.val_lbl, val_str);
    lv_label_set_text(ic.unit_lbl, unit_str);
    lv_color_t col = alert ? C_RED : warn ? C_YELLOW : C_TEXT_PRI;
    lv_obj_set_style_text_color(ic.val_lbl, col, 0);
}