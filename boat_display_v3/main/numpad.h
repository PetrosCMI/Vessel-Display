#pragma once
#include "lvgl.h"

// ---------------------------------------------
//  Numpad modal ΓÇö big-button number entry
//  Pre-built at init time (like keyboard modal)
//  to avoid LVGL invalidation loop on creation.
// ---------------------------------------------

typedef void (*numpad_done_cb)(float value, void* user);

// Build the numpad overlay ΓÇö call once from ui_init()
void numpad_init(void);

// Show numpad with a title and initial value
// decimal_places: 0 = integer, 1 = one decimal place
// cb called with new value on confirm, or NAN on cancel
void numpad_open(const char* title, float initial_value,
                 int decimal_places, float min_val, float max_val,
                 numpad_done_cb cb, void* user);

