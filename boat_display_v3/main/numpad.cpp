// numpad.cpp ΓÇö pre-built big-button number entry modal
#include "numpad.h"
#include "ui.h"
#include "lvgl.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

// -- State -----------------------------------------------------
static lv_obj_t*    s_overlay    = NULL;
static lv_obj_t*    s_title_lbl  = NULL;
static lv_obj_t*    s_value_lbl  = NULL;
static lv_obj_t*    s_hint_lbl   = NULL;
static numpad_done_cb s_cb       = NULL;
static void*        s_user       = NULL;
static int          s_decimals   = 0;
static float        s_min        = 0;
static float        s_max        = 9999;

// Input buffer ΓÇö raw digits as string, e.g. "1234" = 12.34 with 2 decimals
#define MAX_DIGITS 6
static char s_digits[MAX_DIGITS + 1] = {};
static int  s_digit_count = 0;

// -- Convert digit buffer to float ----------------------------
static float digits_to_float(void) {
    if (s_digit_count == 0) return 0.0f;
    int raw = atoi(s_digits);
    if (s_decimals == 0) return (float)raw;
    float div = 1.0f;
    for (int i = 0; i < s_decimals; i++) div *= 10.0f;
    return raw / div;
}

// -- Update the display label ----------------------------------
static void refresh_display(void) {
    if (!s_value_lbl) return;
    float v = digits_to_float();
    char buf[32];
    if (s_decimals == 0)
        snprintf(buf, sizeof(buf), "%.0f", v);
    else if (s_decimals == 1)
        snprintf(buf, sizeof(buf), "%.1f", v);
    else
        snprintf(buf, sizeof(buf), "%.2f", v);
    lv_label_set_text(s_value_lbl, buf);
}

// -- Button handler --------------------------------------------
static void btn_cb(lv_event_t* e) {
    const char* txt = (const char*)lv_obj_get_user_data(
        (lv_obj_t*)lv_event_get_target(e));
    if (!txt) return;

    if (txt[0] >= '0' && txt[0] <= '9') {
        // Digit ΓÇö append if room
        if (s_digit_count < MAX_DIGITS) {
            s_digits[s_digit_count++] = txt[0];
            s_digits[s_digit_count]   = '\0';
            refresh_display();
        }
    } else if (strcmp(txt, "DEL") == 0) {
        if (s_digit_count > 0) {
            s_digits[--s_digit_count] = '\0';
            refresh_display();
        }
    } else if (strcmp(txt, "OK") == 0) {
        float v = digits_to_float();
        if (v < s_min) v = s_min;
        if (v > s_max) v = s_max;
        lv_obj_add_flag(s_overlay, LV_OBJ_FLAG_HIDDEN);
        if (s_cb) s_cb(v, s_user);
        s_cb   = NULL;
        s_user = NULL;
    } else if (strcmp(txt, "X") == 0) {
        lv_obj_add_flag(s_overlay, LV_OBJ_FLAG_HIDDEN);
        if (s_cb) s_cb(NAN, s_user);
        s_cb   = NULL;
        s_user = NULL;
    }
}

// -- Make one numpad button ------------------------------------
static lv_obj_t* make_btn(lv_obj_t* parent, const char* label,
                            lv_color_t bg, int x, int y, int w, int h) {
    lv_obj_t* btn = lv_btn_create(parent);
    lv_obj_set_size(btn, w, h);
    lv_obj_set_pos(btn, x, y);
    lv_obj_set_style_bg_color(btn, bg, 0);
    lv_obj_set_style_radius(btn, 8, 0);

    lv_obj_t* lbl = lv_label_create(btn);
    lv_label_set_text(lbl, label);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_color(lbl, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(lbl);

    // Store label text as user data for the callback
    lv_obj_set_user_data(btn, (void*)label);
    lv_obj_add_event_cb(btn, btn_cb, LV_EVENT_CLICKED, NULL);
    return btn;
}

// -- Init ΓÇö build overlay once ---------------------------------
void numpad_init(void) {
    // Full-screen overlay
    s_overlay = lv_obj_create(lv_screen_active());
    lv_obj_set_size(s_overlay, 480, 480);
    lv_obj_set_pos(s_overlay, 0, 0);
    lv_obj_set_style_bg_color(s_overlay, lv_color_hex(0x050810), 0);
    lv_obj_set_style_bg_opa(s_overlay, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_overlay, 0, 0);
    lv_obj_set_scrollbar_mode(s_overlay, LV_SCROLLBAR_MODE_OFF);

    // Title
    s_title_lbl = lv_label_create(s_overlay);
    lv_label_set_text(s_title_lbl, "");
    lv_obj_set_style_text_color(s_title_lbl, C_ACCENT, 0);
    lv_obj_set_style_text_font(s_title_lbl, &lv_font_montserrat_16, 0);
    lv_obj_set_pos(s_title_lbl, 20, 12);

    // Value display
    s_value_lbl = lv_label_create(s_overlay);
    lv_label_set_text(s_value_lbl, "0");
    lv_obj_set_style_text_color(s_value_lbl, C_TEXT_PRI, 0);
    lv_obj_set_style_text_font(s_value_lbl, &lv_font_montserrat_48, 0);
    lv_obj_set_pos(s_value_lbl, 20, 36);

    // Range hint
    s_hint_lbl = lv_label_create(s_overlay);
    lv_label_set_text(s_hint_lbl, "");
    lv_obj_set_style_text_color(s_hint_lbl, C_TEXT_SEC, 0);
    lv_obj_set_style_text_font(s_hint_lbl, &lv_font_montserrat_12, 0);
    lv_obj_align(s_hint_lbl, LV_ALIGN_TOP_RIGHT, -20, 20);

    // Button grid ΓÇö 4 rows ├ù 3 cols
    // Layout:  7  8  9
    //          4  5  6
    //          1  2  3
    //         DEL 0  OK
    // Plus cancel (X) top-right

    const int BW = 140;   // button width
    const int BH = 82;    // button height
    const int BG = 6;     // gap
    const int OX = 10;    // origin x
    const int OY = 100;   // origin y

    lv_color_t c_num  = lv_color_hex(0x1A2538);
    lv_color_t c_ok   = C_ACCENT;
    lv_color_t c_del  = lv_color_hex(0x303050);
    lv_color_t c_can  = lv_color_hex(0x401010);

    // Row 0: 7 8 9
    make_btn(s_overlay, "7", c_num, OX,           OY,            BW, BH);
    make_btn(s_overlay, "8", c_num, OX+BW+BG,     OY,            BW, BH);
    make_btn(s_overlay, "9", c_num, OX+(BW+BG)*2, OY,            BW, BH);
    // Row 1: 4 5 6
    make_btn(s_overlay, "4", c_num, OX,           OY+(BH+BG),    BW, BH);
    make_btn(s_overlay, "5", c_num, OX+BW+BG,     OY+(BH+BG),    BW, BH);
    make_btn(s_overlay, "6", c_num, OX+(BW+BG)*2, OY+(BH+BG),    BW, BH);
    // Row 2: 1 2 3
    make_btn(s_overlay, "1", c_num, OX,           OY+(BH+BG)*2,  BW, BH);
    make_btn(s_overlay, "2", c_num, OX+BW+BG,     OY+(BH+BG)*2,  BW, BH);
    make_btn(s_overlay, "3", c_num, OX+(BW+BG)*2, OY+(BH+BG)*2,  BW, BH);
    // Row 3: DEL 0 OK
    make_btn(s_overlay, "DEL", c_del, OX,           OY+(BH+BG)*3,  BW, BH);
    make_btn(s_overlay, "0",   c_num, OX+BW+BG,     OY+(BH+BG)*3,  BW, BH);
    make_btn(s_overlay, "OK",  c_ok,  OX+(BW+BG)*2, OY+(BH+BG)*3,  BW, BH);

    // Cancel button top-right
    make_btn(s_overlay, "X", c_can, 440-48, 8, 48, 36);
    // Override font size for cancel ΓÇö smaller
    lv_obj_t* x_btn = lv_obj_get_child(s_overlay,
        lv_obj_get_child_count(s_overlay) - 1);
    if (x_btn) {
        lv_obj_t* x_lbl = lv_obj_get_child(x_btn, 0);
        if (x_lbl)
            lv_obj_set_style_text_font(x_lbl, &lv_font_montserrat_16, 0);
    }

    // Start hidden
    lv_obj_add_flag(s_overlay, LV_OBJ_FLAG_HIDDEN);
}

// -- Open -----------------------------------------------------
void numpad_open(const char* title, float initial_value,
                 int decimal_places, float min_val, float max_val,
                 numpad_done_cb cb, void* user) {
    if (!s_overlay) return;

    s_cb       = cb;
    s_user     = user;
    s_decimals = decimal_places;
    s_min      = min_val;
    s_max      = max_val;

    // Pre-fill digits from initial value
    s_digit_count = 0;
    memset(s_digits, 0, sizeof(s_digits));
    if (!isnan(initial_value) && initial_value > 0) {
        char tmp[16];
        if (decimal_places == 0)
            snprintf(tmp, sizeof(tmp), "%.0f", initial_value);
        else if (decimal_places == 1)
            snprintf(tmp, sizeof(tmp), "%.0f", initial_value * 10.0f);
        else
            snprintf(tmp, sizeof(tmp), "%.0f", initial_value * 100.0f);
        // Copy only digits
        for (int i = 0; tmp[i] && s_digit_count < MAX_DIGITS; i++) {
            if (tmp[i] >= '0' && tmp[i] <= '9')
                s_digits[s_digit_count++] = tmp[i];
        }
        s_digits[s_digit_count] = '\0';
    }

    lv_label_set_text(s_title_lbl, title ? title : "");

    // Range hint
    char hint[32];
    if (decimal_places == 0)
        snprintf(hint, sizeof(hint), "%.0f ΓÇô %.0f", min_val, max_val);
    else
        snprintf(hint, sizeof(hint), "%.1f ΓÇô %.1f", min_val, max_val);
    lv_label_set_text(s_hint_lbl, hint);

    refresh_display();

    lv_obj_clear_flag(s_overlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_overlay);
}

