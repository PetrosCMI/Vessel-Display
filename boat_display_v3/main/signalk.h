#pragma once

// Start the SignalK WebSocket task — call once after WiFi is connected
void signalk_start(void);

// Stop and clean up (optional, for graceful shutdown)
void signalk_stop(void);
