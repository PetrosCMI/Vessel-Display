#pragma once

// ---------------------------------------------
//  Alarm overlay ΓÇö full-screen modal shown when
//  any alarm fires. Dismissed by ACK or SILENCE.
// ---------------------------------------------

// Build overlay once during ui_init()
void alarm_overlay_init(void);

// Called from ui_update() ΓÇö shows/hides/refreshes overlay
void alarm_overlay_update(void);
