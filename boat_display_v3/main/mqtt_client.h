#pragma once

// MQTT broker connection API
void mqtt_start(void);
void mqtt_stop(void);

// MQTT topic paths for NMEA data (from SignalK via MQTT)
// These match SignalK's default MQTT topic structure: signalk/vessels/self
#define MQTT_BASE "vessels/PoleDancer/"

// Navigation topics
#define MQTT_SOG        MQTT_BASE "navigation/speedOverGround"
#define MQTT_STW        MQTT_BASE "navigation/speedThroughWater"
#define MQTT_COG        MQTT_BASE "navigation/courseOverGroundTrue"
#define MQTT_HDG_TRUE   MQTT_BASE "navigation/headingTrue"
#define MQTT_HDG_MAG    MQTT_BASE "navigation/headingMagnetic"
#define MQTT_POSITION   MQTT_BASE "navigation/position"
#define MQTT_DEPTH      MQTT_BASE "environment/depth/belowTransducer"

// Wind topics
#define MQTT_AWS        MQTT_BASE "environment/wind/speedApparent"
#define MQTT_AWA        MQTT_BASE "environment/wind/angleApparent"
#define MQTT_TWS        MQTT_BASE "environment/wind/speedTrue"
#define MQTT_TWA        MQTT_BASE "environment/wind/angleTrue"
#define MQTT_TWD        MQTT_BASE "environment/wind/directionTrue"

// Environment topics
#define MQTT_WATER_TEMP MQTT_BASE "environment/water/temperature"
#define MQTT_AIR_TEMP   MQTT_BASE "environment/outside/temperature"
#define MQTT_PRESSURE   MQTT_BASE "environment/outside/pressure"

// Battery topics (from your ESP32 battery monitors)
#define MQTT_BATT_HOUSE     MQTT_BASE "electrical/battery/house"
#define MQTT_BATT_HOUSE_LI  MQTT_BASE "electrical/battery/house/li1"
#define MQTT_BATT_START     MQTT_BASE "electrical/battery/starter"
#define MQTT_BATT_FWD       MQTT_BASE "electrical/battery/forward"
