#pragma once

// ---------------------------------------------
//  WiFi config AP + web server
// ---------------------------------------------

#define CONFIG_AP_SSID     "BoatDisplay"
#define CONFIG_AP_PASSWORD ""            // open AP ΓÇö no password needed
#define CONFIG_AP_IP       "192.168.4.1"
#define CONFIG_AP_PORT     80

// Start AP + web config server
// Blocks until user saves config or timeout (0 = no timeout)
void wifi_config_start(void);

// Stop AP + web config server
void wifi_config_stop(void);

// Returns true if AP config mode is currently active
bool wifi_config_active(void);
