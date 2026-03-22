#pragma once
#include <stdint.h>

// ---------------------------------------------
//  Display timeout & wake
// ---------------------------------------------

// Call once after display is initialised
void display_timeout_init(void);

// Call on every touch event to reset the timeout timer
void display_timeout_reset(void);

// Wake display immediately (call when alarm fires)
void display_wake(void);

// Returns true if display is currently dimmed or off
bool display_is_sleeping(void);

