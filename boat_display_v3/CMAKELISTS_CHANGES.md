# CMakeLists.txt Changes

## What Changed

### Added to SRCS (source files):
```cmake
"boat_mqtt.cpp"
```

### Added to REQUIRES (component dependencies):
```cmake
esp-mqtt
```

## Full Section
```cmake
idf_component_register(
    SRCS
        "main.cpp"
        "boat_data.cpp"
        "settings.cpp"
        "signalk.cpp"
        "boat_mqtt.cpp"          # ← NEW
        "alarm.cpp"
        # ... other files ...
    
    REQUIRES
        esp_wifi
        nvs_flash
        esp_event
        esp_netif
        esp_websocket_client
        esp-mqtt                  # ← NEW
        esp_codec_dev
        driver
        freertos
        log
        esp_http_server
)
```

## Why These Changes?

1. **boat_mqtt.cpp** - Our new MQTT client implementation needs to be compiled
2. **esp-mqtt** - ESP-IDF's MQTT component (from component registry)

## Note
The `esp-mqtt` component will be downloaded from the Espressif component registry when you first build.
