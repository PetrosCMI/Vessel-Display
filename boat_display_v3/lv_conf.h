/**
 * lv_conf.h — LVGL configuration for boat display
 * Place in main/ alongside the other source files.
 * The managed LVGL component picks this up via LV_CONF_PATH or
 * the include path set in main/CMakeLists.txt.
 */
#ifndef LV_CONF_H
#define LV_CONF_H

#include <stdint.h>

/* Colour depth */
#define LV_COLOR_DEPTH 16

/* Memory — use PSRAM for LVGL heap so we're not limited to internal RAM.
   2MB gives plenty of headroom for widgets, styles and draw buffers.    */
#define LV_MEM_SIZE       (2U * 1024U * 1024U)   /* 2 MB from PSRAM     */
#define LV_MEM_ADR        0                        /* 0 = use lv_malloc   */
#define LV_MEM_POOL_INCLUDE "esp_heap_caps.h"
#define LV_MEM_POOL_ALLOC(size) heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)
#define LV_MEM_POOL_FREE(p)     heap_caps_free(p)

/* Tick */
#define LV_TICK_CUSTOM 0  /* BSP/lvgl_port provides the tick */

/* ── Fonts ───────────────────────────────────────────────────
   Enable every size used anywhere in the project.
   Missing a size here = "not declared in this scope" error.    */
#define LV_FONT_MONTSERRAT_12  1
#define LV_FONT_MONTSERRAT_14  1   /* default — must stay enabled */
#define LV_FONT_MONTSERRAT_16  1   /* section headers, labels    */
#define LV_FONT_MONTSERRAT_24  1
#define LV_FONT_MONTSERRAT_32  1   /* medium values              */
#define LV_FONT_MONTSERRAT_48  1   /* large instrument values    */

#define LV_FONT_DEFAULT  &lv_font_montserrat_14

/* ── Widgets needed by the settings page ────────────────────  */
#define LV_USE_BTNMATRIX  1
#define LV_USE_SWITCH     1
#define LV_USE_SLIDER     1
#define LV_USE_SPINBOX    1
#define LV_USE_ARC        1
#define LV_USE_CANVAS     1
#define LV_USE_TABVIEW    1
#define LV_USE_LABEL      1
#define LV_USE_BTN        1
#define LV_USE_IMG        1
#define LV_USE_KEYBOARD   1
#define LV_USE_TEXTAREA   1

/* ── Logging ─────────────────────────────────────────────── */
#define LV_USE_LOG       1
#define LV_LOG_LEVEL     LV_LOG_LEVEL_WARN
#define LV_LOG_PRINTF    1

/* ── Animation ───────────────────────────────────────────── */
#define LV_USE_ANIMATION 1

#endif /* LV_CONF_H */