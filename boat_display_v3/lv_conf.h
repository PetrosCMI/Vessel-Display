/**
 * lv_conf.h ΓÇö LVGL configuration for boat display
 * Place in main/ alongside the other source files.
 * The managed LVGL component picks this up via LV_CONF_PATH or
 * the include path set in main/CMakeLists.txt.
 */
#ifndef LV_CONF_H
#define LV_CONF_H

#include <stdint.h>

/* Colour depth */
#define LV_COLOR_DEPTH 16

/* Memory */
/* Memory — controlled via sdkconfig CONFIG_LV_MEM_SIZE_KILOBYTES=96 */

/* Tick */
#define LV_TICK_CUSTOM 0  /* BSP/lvgl_port provides the tick */

/* ΓöÇΓöÇ Fonts ΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇ
   Enable every size used anywhere in the project.
   Missing a size here = "not declared in this scope" error.    */
#define LV_FONT_MONTSERRAT_12  1
#define LV_FONT_MONTSERRAT_14  1   /* default ΓÇö must stay enabled */
#define LV_FONT_MONTSERRAT_16  1   /* section headers, labels    */
#define LV_FONT_MONTSERRAT_24  1
#define LV_FONT_MONTSERRAT_32  1   /* medium values              */
#define LV_FONT_MONTSERRAT_48  1   /* large instrument values    */

#define LV_FONT_DEFAULT  &lv_font_montserrat_14

/* ΓöÇΓöÇ Widgets needed by the settings page ΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇ  */
#define LV_USE_BTNMATRIX  1
#define LV_USE_SWITCH     1
#define LV_USE_SLIDER     1
#define LV_USE_SPINBOX    1
#define LV_USE_ARC        1
#define LV_USE_TABVIEW    1
#define LV_USE_LABEL      1
#define LV_USE_BTN        1
#define LV_USE_IMG        1
#define LV_USE_KEYBOARD   1
#define LV_USE_TEXTAREA   1

/* ΓöÇΓöÇ Logging ΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇ */
#define LV_USE_LOG       1
#define LV_LOG_LEVEL     LV_LOG_LEVEL_WARN
#define LV_LOG_PRINTF    1

/* ΓöÇΓöÇ Animation ΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇΓöÇ */
#define LV_USE_ANIMATION 1

// Reduce default refresh period — less CPU per second
#define LV_DEF_REFR_PERIOD  33   // ~30fps instead of default 16ms (60fps)

#endif /* LV_CONF_H */
