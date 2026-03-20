#ifndef LV_CONF_H
#define LV_CONF_H

#include <stdint.h>

#define LV_COLOR_DEPTH 16
#define LV_COLOR_16_SWAP 0

/* Memory */
#define LV_MEM_CUSTOM 0
#define LV_MEM_SIZE (128U * 1024U)   /* 128 KB from PSRAM */

/* HAL */
#define LV_TICK_CUSTOM 1
#define LV_TICK_CUSTOM_INCLUDE <Arduino.h>
#define LV_TICK_CUSTOM_SYS_TIME_EXPR (millis())

/* Fonts */
#define LV_FONT_MONTSERRAT_12 1
#define LV_FONT_MONTSERRAT_16 1
#define LV_FONT_MONTSERRAT_24 1
#define LV_FONT_MONTSERRAT_32 1
#define LV_FONT_MONTSERRAT_48 1
#define LV_FONT_DEFAULT &lv_font_montserrat_24

/* Features */
#define LV_USE_TABVIEW  1
#define LV_USE_LABEL    1
#define LV_USE_ARC      1
#define LV_USE_LINE     1
#define LV_USE_IMG      1
#define LV_USE_METER    1
#define LV_USE_SPINNER  1
#define LV_USE_MSGBOX   1

/* Logging */
#define LV_USE_LOG      1
#define LV_LOG_LEVEL    LV_LOG_LEVEL_WARN
#define LV_LOG_PRINTF   1

#define LV_USE_PERF_MONITOR 0
#define LV_USE_MEM_MONITOR  0

/* Animation */
#define LV_USE_ANIMATION 1

#endif /* LV_CONF_H */
