#pragma once

// ─────────────────────────────────────────────
//  USER CONFIGURATION  — edit these values
// ─────────────────────────────────────────────

#define POLEDANCER 0

#if POLEDANCER
    // WiFi credentials
    #define WIFI_SSID        "pdair"
    #define WIFI_PASSWORD    "nevermore"

    // SignalK server  (hostname or IP, port, no trailing slash)
    #define SIGNALK_HOST     "192.168.4.4"
    #define SIGNALK_PORT     3000
#else
    // WiFi credentials
    #define WIFI_SSID        "CMIMain"
    #define WIFI_PASSWORD    "Waterfr0ntCl0ud$"

    // SignalK server  (hostname or IP, port, no trailing slash)
    #define SIGNALK_HOST     "10.12.0.113"
    #define SIGNALK_PORT     3000
#endif

// Display brightness 0–255
#define LCD_BACKLIGHT_BRIGHTNESS 200

// ─────────────────────────────────────────────
//  WAVESHARE ESP32-S3-Touch-LCD-4B  PIN MAP
//  (ST7701 via SW-SPI, RGB parallel panel)
// ─────────────────────────────────────────────

// RGB panel
#define LCD_DE    45
#define LCD_VSYNC 46
#define LCD_HSYNC 40
#define LCD_PCLK  21

// RGB data  (R5..R1, G6..G1, B5..B1)
#define LCD_R0  14
#define LCD_R1  21   // Note: PCLK=21 on some revisions, check your board silk
#define LCD_R2  47
#define LCD_R3  48
#define LCD_R4  45
#define LCD_G0  9
#define LCD_G1  10
#define LCD_G2  11
#define LCD_G3  12
#define LCD_G4  13
#define LCD_G5  14
#define LCD_B0  4
#define LCD_B1  5
#define LCD_B2  6
#define LCD_B3  7
#define LCD_B4  15

// ST7701 init via SW-SPI
#define LCD_CS    -1
#define LCD_SCK   47
#define LCD_SDA   41
#define LCD_RST   -1

// Backlight (PWM)
#define LCD_BL    2

// GT911 touch (I2C)
#define TOUCH_SDA  8
#define TOUCH_SCL  3
#define TOUCH_INT  -1
#define TOUCH_RST  -1
#define TOUCH_ADDR 0x5D   // or 0x14 — probe both

// ─────────────────────────────────────────────
//  DISPLAY GEOMETRY
// ─────────────────────────────────────────────
#define DISPLAY_WIDTH   480
#define DISPLAY_HEIGHT  480

// LVGL draw buffer lines (larger = faster, more RAM)
#define LV_BUF_LINES    120
