#pragma once
/**
 * PSRAM-fallback allocator for LVGL.
 *
 * LVGL's default malloc/realloc/free only return from internal heap on the
 * ESP32-S3 (about 300 KB after WiFi starts). Under audio streaming + large
 * layer-buffer requests (e.g. 510x10 RGB565 = 20 KB) the internal heap
 * fragments and allocation fails, producing
 *   lv_draw_buf_create_ex: No memory: ...
 *   Allocating layer buffer failed. Try later
 *
 * These wrappers try internal heap first (fast SRAM) and fall back to
 * PSRAM (8 MB, slower) on failure. free() and realloc() detect which heap
 * the pointer came from automatically via heap_caps_*.
 *
 * Included via LV_MEM_CUSTOM_INCLUDE from lv_conf.h.
 */
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void* lv_psram_alloc(size_t size);
void  lv_psram_free(void* ptr);
void* lv_psram_realloc(void* ptr, size_t new_size);

#ifdef __cplusplus
}
#endif
