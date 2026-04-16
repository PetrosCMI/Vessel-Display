idf_component_register(
    SRCS "esp32_s3_touch_lcd_4b.c"
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "priv_include"
    # Changed 'esp_driver_i2c' and 'esp_driver_gpio' to 'driver'
    REQUIRES driver esp_lcd
    PRIV_REQUIRES esp_timer spiffs fatfs
)