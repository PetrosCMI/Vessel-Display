#pragma once

// ─────────────────────────────────────────────────────────────
//  SignalK path definitions
//  All paths used in signalk.cpp are defined here for easy
//  customisation without touching the parser logic.
// ─────────────────────────────────────────────────────────────

// ── Navigation ───────────────────────────────────────────────
#define SK_SOG              "navigation.speedOverGround"
#define SK_STW              "navigation.speedThroughWater"
#define SK_COG              "navigation.courseOverGroundTrue"
#define SK_HDG_TRUE         "navigation.headingTrue"
#define SK_HDG_MAG          "navigation.headingMagnetic"
#define SK_POSITION         "navigation.position"

// ── Wind ─────────────────────────────────────────────────────
#define SK_AWS              "environment.wind.speedApparent"
#define SK_AWA              "environment.wind.angleApparent"
#define SK_TWS              "environment.wind.speedTrue"
#define SK_TWA              "environment.wind.angleTrueWater"
#define SK_TWD              "environment.wind.directionTrue"

// ── Depth / Environment ──────────────────────────────────────
#define SK_DEPTH            "environment.depth.belowTransducer"
#define SK_WATER_TEMP       "environment.water.temperature"
#define SK_AIR_TEMP         "environment.outside.temperature"

// ── Propulsion ───────────────────────────────────────────────
#define SK_RPM              "propulsion.main.revolutions"
#define SK_COOLANT_TEMP     "propulsion.main.temperature"
#define SK_OIL_PRESSURE     "propulsion.main.oilPressure"

// ── Electrical ───────────────────────────────────────────────
#define SK_BATT_HOUSE_V     "electrical.batteries.house.voltage"
#define SK_BATT_HOUSE_A_ALL "electrical.batteries.house.all.current"
#define SK_BATT_HOUSE_A_LI  "electrical.batteries.house.li.current"
#define SK_BATT_START_V     "electrical.batteries.start.voltage"
#define SK_BATT_START_A     "electrical.batteries.start.current"
#define SK_BATT_FWD_V       "electrical.batteries.forward.voltage"
