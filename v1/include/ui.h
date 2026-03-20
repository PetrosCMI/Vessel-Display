#pragma once

// Initialise the LVGL UI (call after lv_init and display driver setup)
void ui_init();

// Refresh all gauge values from gBoat (call every ~250 ms from main loop)
void ui_update();
