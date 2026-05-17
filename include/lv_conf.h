/**
 * lv_conf.h — LVGL 9.2.2 configuration
 * Place in the 'include/' directory so LVGL finds it via LV_CONF_INCLUDE_SIMPLE.
 */

#if 1  /* Set to 1 to enable */
#ifndef LV_CONF_H
#define LV_CONF_H

#include <stdint.h>

/* ==========================================================================
 * COLOR SETTINGS
 * ========================================================================== */
#define LV_COLOR_DEPTH 16          /* RGB565 for the ST7262 panel */

/* ==========================================================================
 * MEMORY SETTINGS
 * ========================================================================== */
#define LV_MEM_CUSTOM    1
#if LV_MEM_CUSTOM
    #define LV_MEM_CUSTOM_INCLUDE  <stdlib.h>
    #define LV_MEM_CUSTOM_ALLOC    malloc
    #define LV_MEM_CUSTOM_FREE     free
    #define LV_MEM_CUSTOM_REALLOC  realloc
#else
    #define LV_MEM_SIZE  (512U * 1024U)  /* 512 KB internal arena */
#endif

/* ==========================================================================
 * HAL / TICK
 * ========================================================================== */
#define LV_DEF_REFR_PERIOD  10     /* [ms] default display refresh */

/* Custom tick source — no need to call lv_tick_inc() manually */
#define LV_TICK_CUSTOM          1
#define LV_TICK_CUSTOM_INCLUDE  "Arduino.h"
#define LV_TICK_CUSTOM_SYS_TIME_EXPR  (millis())

/* ==========================================================================
 * LOGGING
 * ========================================================================== */
#define LV_USE_LOG      1
#define LV_LOG_LEVEL    LV_LOG_LEVEL_WARN
#define LV_LOG_PRINTF   1

/* ==========================================================================
 * DRAW / RENDER
 * ========================================================================== */
// LV_DRAW_BUF_STRIDE_ALIGN left at default (1 = no padding).
// With STRIDE_ALIGN>1, LVGL pads row strides for odd-pixel-width areas,
// but the flush callback assumes stride = area_w*2.  Padding causes row
// mis-alignment and the 'alternating garbled/correct' artefact.
#define LV_DRAW_BUF_ALIGN          4

/* ==========================================================================
 * FONT USAGE
 * ========================================================================== */
#define LV_FONT_MONTSERRAT_14  1
#define LV_FONT_MONTSERRAT_16  1
#define LV_FONT_MONTSERRAT_20  1
#define LV_FONT_MONTSERRAT_24  1
#define LV_FONT_MONTSERRAT_32  1
#define LV_FONT_MONTSERRAT_48  1
#define LV_FONT_DEFAULT        (&lv_font_montserrat_14)

/* ==========================================================================
 * LAYOUTS
 * ========================================================================== */
#define LV_USE_FLEX  1
#define LV_USE_GRID  1

/* ==========================================================================
 * WIDGETS
 * ========================================================================== */
#define LV_USE_ARC        1
#define LV_USE_BAR        1
#define LV_USE_BTN        1
#define LV_USE_BTNMATRIX  1
#define LV_USE_CALENDAR   1
#define LV_USE_CANVAS     1
#define LV_USE_CHART      1
#define LV_USE_CHECKBOX   1
#define LV_USE_DROPDOWN   1
#define LV_USE_IMG        1
#define LV_USE_IMAGEBUTTON 1
#define LV_USE_LABEL      1
#define LV_USE_LED        1
#define LV_USE_LINE       1
#define LV_USE_LIST       1
#define LV_USE_MENU       1
#define LV_USE_MSGBOX     1
#define LV_USE_ROLLER     1
#define LV_USE_SCALE      1
#define LV_USE_SLIDER     1
#define LV_USE_SPAN       1
#define LV_USE_SPINBOX    1
#define LV_USE_SPINNER    1
#define LV_USE_SWITCH     1
#define LV_USE_TABLE      1
#define LV_USE_TABVIEW    1
#define LV_USE_TEXTAREA   1
#define LV_USE_TILEVIEW   1
#define LV_USE_WIN        1

/* ==========================================================================
 * THEMES
 * ========================================================================== */
#define LV_USE_THEME_DEFAULT  1
#define LV_THEME_DEFAULT_DARK 1   /* dark theme */

/* ==========================================================================
 * FILE SYSTEM (needed for lv_img with file paths)
 * ========================================================================== */
#define LV_USE_FS_STDIO  0
#define LV_USE_FS_POSIX  0

#endif  /* LV_CONF_H */
#endif  /* Enable content */
