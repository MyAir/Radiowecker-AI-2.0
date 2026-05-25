// PSRAM-fallback allocator for LVGL 9.x.
//
// lv_conf.h sets LV_USE_STDLIB_MALLOC = LV_STDLIB_CUSTOM, which tells LVGL
// to call externally-provided lv_malloc_core / lv_realloc_core /
// lv_free_core (plus a couple of monitor / pool stubs).  Each call tries
// internal SRAM first (fast, ~300 KB after WiFi) and falls back to PSRAM
// (8 MB OPI, slower) on failure.
//
// Without this shim LVGL would use its 64 KB built-in TLSF pool, which is
// too small to decode the OWM weather PNG icons via lodepng -> ARGB8888
// (50x50 = 10 KB plus IDAT scratch buffers).

#include "lv_alloc.h"
#include <lvgl.h>
#include <esp_heap_caps.h>
#include <string.h>

extern "C" {

void lv_mem_init(void)   { /* nothing to init */ }
void lv_mem_deinit(void) { /* nothing to deinit */ }

lv_mem_pool_t lv_mem_add_pool(void* mem, size_t bytes) {
    (void)mem; (void)bytes;
    return nullptr;  // not supported
}

void lv_mem_remove_pool(lv_mem_pool_t pool) {
    (void)pool;  // not supported
}

void* lv_malloc_core(size_t size) {
    // Try internal SRAM first (fast).
    void* p = heap_caps_malloc(size, MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL);
    if (p) return p;
    // Fallback to PSRAM (slower but plentiful).
    return heap_caps_malloc(size, MALLOC_CAP_8BIT | MALLOC_CAP_SPIRAM);
}

void lv_free_core(void* ptr) {
    heap_caps_free(ptr);
}

void* lv_realloc_core(void* ptr, size_t new_size) {
    if (!ptr) return lv_malloc_core(new_size);
    if (new_size == 0) { heap_caps_free(ptr); return nullptr; }
    // heap_caps_realloc with INTERNAL caps tries the same heap first; if
    // the block lives in PSRAM or INTERNAL is exhausted it returns NULL,
    // and we retry against PSRAM.
    void* p = heap_caps_realloc(ptr, new_size, MALLOC_CAP_8BIT | MALLOC_CAP_INTERNAL);
    if (p) return p;
    return heap_caps_realloc(ptr, new_size, MALLOC_CAP_8BIT | MALLOC_CAP_SPIRAM);
}

void lv_mem_monitor_core(lv_mem_monitor_t* mon_p) {
    if (mon_p) memset(mon_p, 0, sizeof(*mon_p));
}

lv_result_t lv_mem_test_core(void) {
    return LV_RESULT_OK;
}

// --- Legacy shim names kept for any caller that still uses them. --------
void* lv_psram_alloc(size_t size)           { return lv_malloc_core(size); }
void  lv_psram_free(void* ptr)              { lv_free_core(ptr); }
void* lv_psram_realloc(void* ptr, size_t n) { return lv_realloc_core(ptr, n); }

} // extern "C"
