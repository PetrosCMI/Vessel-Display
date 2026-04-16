#pragma once
#include <Arduino.h>

// Start the SignalK WebSocket task (call once from setup())
void signalk_start();

// Called from main loop to drive the WebSocket library
void signalk_loop();
