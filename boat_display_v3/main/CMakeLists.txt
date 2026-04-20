idf_component_register(
    SRCS
        "main.cpp"
        "boat_data.cpp"
        "settings.cpp"
        "signalk.cpp"
        "mqtt_client.cpp"
        "alarm.cpp"
        "ui.cpp"
        "ui_helpers.cpp"
        "page_registry.cpp"
        "wifi_config.cpp"

        "page_network.cpp"
        "page_settings.cpp"
        "page_nav.cpp"
        "page_elec.cpp"
        "page_wind.cpp"
        "page_anchor.cpp"
        "page_depth.cpp"

        "display_timeout.cpp"
        "numpad.cpp"
        "alarm_overlay.cpp"
    INCLUDE_DIRS
        "."          # <-- this makes lv_conf.h discoverable by LVGL
    REQUIRES
        esp_wifi
        nvs_flash
        esp_event
        esp_netif
        esp_websocket_client
        esp_codec_dev
        driver
        freertos
        log
        esp_http_server
)

# Tell LVGL to look for lv_conf.h via our include path
# (the managed component respects LV_CONF_PATH or the parent include dirs)
target_compile_definitions(${COMPONENT_LIB} PUBLIC
    LV_CONF_INCLUDE_SIMPLE=1
)
