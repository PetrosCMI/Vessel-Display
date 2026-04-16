#include <Arduino.h>
#include <WiFi.h>
#include <lvgl.h>

// GFX library for ST7701 RGB panel + GT911 touch
#include <Arduino_GFX_Library.h>
#include <TAMC_GT911.h>

#include "config.h"
#include "boat_data.h"
#include "signalk.h"
#include "ui.h"

// ─────────────────────────────────────────────
//  Global boat data + mutex
// ─────────────────────────────────────────────
BoatData gBoat;
SemaphoreHandle_t gBoatMutex = NULL;

// ─────────────────────────────────────────────
//  Display driver  (ST7701 + RGB panel)
//  Waveshare ESP32-S3-Touch-LCD-4B
//  480x480, RGB565
// ─────────────────────────────────────────────
// Arduino_SWSPI::Arduino_SWSPI(int8_t dc, int8_t cs, int8_t sck, int8_t mosi, int8_t miso /* = GFX_NOT_DEFINED */)
//    : _dc(dc), _cs(cs), _sck(sck), _mosi(mosi), _miso(miso)
Arduino_DataBus* bus = new Arduino_SWSPI(
    GFX_NOT_DEFINED, // DC not used for ST7701 SW-SPI init
    LCD_CS,          // CS
    LCD_SCK,         // SCK
    LCD_SDA,         // MOSO
    GFX_NOT_DEFINED  // MISO not needed
);

// Arduino_XCA9554SWSPI::Arduino_XCA9554SWSPI(int8_t rst, int8_t cs, int8_t sck, int8_t mosi, TwoWire *wire, uint8_t i2c_addr)
//    : _rst(rst), _cs(cs), _sck(sck), _mosi(mosi), _wire(wire), _i2c_addr(i2c_addr)

Arduino_XCA9554SWSPI *expander = new Arduino_XCA9554SWSPI(
  7,    /* DC */
  0,    /* CS */
  2,    /* SCK */
  1,    /* SDA*/
  &Wire,
  0x20);


Arduino_ESP32RGBPanel* rgbpanel = new Arduino_ESP32RGBPanel(
    LCD_DE, LCD_VSYNC, LCD_HSYNC, LCD_PCLK,
    LCD_R0, LCD_R1, LCD_R2, LCD_R3, LCD_R4,
    LCD_G0, LCD_G1, LCD_G2, LCD_G3, LCD_G4, LCD_G5,
    LCD_B0, LCD_B1, LCD_B2, LCD_B3, LCD_B4,
    1 /* hsync_polarity */,  40 /* hsync_front_porch */,
    2 /* hsync_pulse_width */, 44 /* hsync_back_porch */,
    1 /* vsync_polarity */,  16 /* vsync_front_porch */,
    2 /* vsync_pulse_width */, 18 /* vsync_back_porch */
);

Arduino_ST7701_RGBPanel* gfx = new Arduino_ST7701_RGBPanel(
    rgbpanel, bus, LCD_RST,
    false /* IPS */,
    DISPLAY_WIDTH, DISPLAY_HEIGHT
);

Arduino_ESP32RGBPanel *rgbpanel = new Arduino_ESP32RGBPanel(
  17 /* DE */, 3 /* VSYNC */, 46 /* HSYNC */, 9 /* PCLK */,
  10 /* B0 */, 11 /* B1 */, 12 /* B2 */, 13 /* B3 */, 14 /* B4 */,
  21 /* G0 */, 8 /* G1 */, 18 /* G2 */, 45 /* G3 */, 38 /* G4 */, 39 /* G5 */,
  40 /* R0 */, 41 /* R1 */, 42 /* R2 */, 2 /* R3 */, 1 /* R4 */,
  1 /* hsync_polarity */, 10 /* hsync_front_porch */, 8 /* hsync_pulse_width */, 50 /* hsync_back_porch */,
  1 /* vsync_polarity */, 10 /* vsync_front_porch */, 8 /* vsync_pulse_width */, 20 /* vsync_back_porch */);

  Arduino_RGB_Display *gfx = new Arduino_RGB_Display(
  480 /* width */, 480 /* height */, rgbpanel, 0 /* rotation */, true /* auto_flush */,
  expander, GFX_NOT_DEFINED /* RST */, st7701_type1_init_operations, sizeof(st7701_type1_init_operations));

// ─────────────────────────────────────────────
//  GT911 touch
// ─────────────────────────────────────────────
TAMC_GT911 touch(TOUCH_SDA, TOUCH_SCL, TOUCH_INT, TOUCH_RST,
                 DISPLAY_WIDTH, DISPLAY_HEIGHT);

// ─────────────────────────────────────────────
//  LVGL buffers  (double-buffer in PSRAM)
// ─────────────────────────────────────────────
static lv_disp_draw_buf_t draw_buf;
static lv_color_t* buf1 = nullptr;
static lv_color_t* buf2 = nullptr;

// LVGL flush callback
static void lvgl_flush_cb(lv_disp_drv_t* drv, const lv_area_t* area, lv_color_t* px_map) {
    gfx->draw16bitRGBBitmap(area->x1, area->y1,
                             (uint16_t*)px_map,
                             area->x2 - area->x1 + 1,
                             area->y2 - area->y1 + 1);
    lv_disp_flush_ready(drv);
}

// LVGL touch read callback
static void lvgl_touch_cb(lv_indev_drv_t* drv, lv_indev_data_t* data) {
    touch.read();
    if (touch.isTouched && touch.touches > 0) {
        data->state = LV_INDEV_STATE_PRESSED;
        data->point.x = touch.points[0].x;
        data->point.y = touch.points[0].y;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

// ─────────────────────────────────────────────
//  WiFi setup with retry
// ─────────────────────────────────────────────
static void wifi_connect() {
    Serial.printf("[WiFi] Connecting to %s", WIFI_SSID);
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    uint32_t t = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - t < 15000) {
        delay(500);
        Serial.print(".");
        lv_timer_handler();   // keep screen alive during connect
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("\n[WiFi] Connected — IP: %s\n",
                       WiFi.localIP().toString().c_str());
    } else {
        Serial.println("\n[WiFi] Failed — will retry in background");
    }
}

// ─────────────────────────────────────────────
//  setup()
// ─────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    Serial.println("\n[BOOT] Boat Display starting...");

    // Mutex for shared boat data
    gBoatMutex = xSemaphoreCreateMutex();

    // Backlight
    ledcSetup(0, 5000, 8);
    ledcAttachPin(LCD_BL, 0);
    ledcWrite(0, LCD_BACKLIGHT_BRIGHTNESS);

    // Init GFX
    if (!gfx->begin()) {
        Serial.println("[ERROR] GFX init failed");
        while (1) delay(100);
    }
    gfx->fillScreen(BLACK);
    Serial.println("[GFX] Display OK");

    // Init touch
    Wire.begin(TOUCH_SDA, TOUCH_SCL);
    touch.begin();
    touch.setRotation(ROTATION_NORMAL);
    Serial.println("[TOUCH] GT911 OK");

    // Init LVGL
    lv_init();

    // Allocate double buffer in PSRAM
    size_t buf_size = DISPLAY_WIDTH * LV_BUF_LINES;
    buf1 = (lv_color_t*)heap_caps_malloc(buf_size * sizeof(lv_color_t), MALLOC_CAP_SPIRAM);
    buf2 = (lv_color_t*)heap_caps_malloc(buf_size * sizeof(lv_color_t), MALLOC_CAP_SPIRAM);
    if (!buf1 || !buf2) {
        Serial.println("[ERROR] LVGL buffer alloc failed — trying internal RAM");
        buf1 = (lv_color_t*)heap_caps_malloc(buf_size * sizeof(lv_color_t), MALLOC_CAP_DMA);
        buf2 = (lv_color_t*)heap_caps_malloc(buf_size * sizeof(lv_color_t), MALLOC_CAP_DMA);
    }
    lv_disp_draw_buf_init(&draw_buf, buf1, buf2, buf_size);

    // Register display driver
    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res  = DISPLAY_WIDTH;
    disp_drv.ver_res  = DISPLAY_HEIGHT;
    disp_drv.flush_cb = lvgl_flush_cb;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);

    // Register touch input driver
    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type    = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = lvgl_touch_cb;
    lv_indev_drv_register(&indev_drv);

    Serial.println("[LVGL] Initialised");

    // Build UI
    ui_init();
    Serial.println("[UI] Built");

    // Connect WiFi
    wifi_connect();

    // Start SignalK WebSocket
    signalk_start();

    Serial.println("[BOOT] Setup complete");
}

// ─────────────────────────────────────────────
//  loop()
// ─────────────────────────────────────────────
static uint32_t last_ui_update  = 0;
static uint32_t last_wifi_check = 0;

void loop() {
    // Drive LVGL
    lv_timer_handler();

    // Drive WebSocket
    signalk_loop();

    uint32_t now = millis();

    // Update UI values every 250 ms
    if (now - last_ui_update >= 250) {
        last_ui_update = now;
        ui_update();
    }

    // Reconnect WiFi if dropped
    if (now - last_wifi_check >= 10000) {
        last_wifi_check = now;
        if (WiFi.status() != WL_CONNECTED) {
            Serial.println("[WiFi] Reconnecting...");
            WiFi.reconnect();
        }
    }

    delay(5);  // Yield — keeps WDT happy
}
