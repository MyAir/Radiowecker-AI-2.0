#include "DisplayManager.h"
#include "../config.h"
#include <esp_heap_caps.h>
#include <freertos/semphr.h>
#include <rom/cache.h>

// ---------------------------------------------------------------------------
// Touch state cache  (written by main loop, read by render task via _lvglTouch)
// ---------------------------------------------------------------------------
static volatile bool     s_touch_pressed = false;
static volatile uint16_t s_touch_x       = 0;
static volatile uint16_t s_touch_y       = 0;

// ---------------------------------------------------------------------------
// VSYNC semaphore — given by lgfx_vsync_callback() at every VSYNC_END ISR.
// DisplayManager::loop() takes it (non-blocking) before calling lv_refr_now().
// This synchronises rendering to the DMA scan start, ensuring the cache flush
// completes before the GDMA reaches the just-rendered dirty region.
// ---------------------------------------------------------------------------
static SemaphoreHandle_t  s_vsync_sem   = nullptr;
static volatile uint32_t  s_vsync_count = 0; // incremented in ISR for diagnostics

// ---------------------------------------------------------------------------
// Double-buffer writeback
//
// Bus_RGB allocates two PSRAM framebuffers (_frame_buffer = front,
// _frame_buffer2 = back).  At every VSYNC_END the ISR swaps them if
// requestSwap() was called.
//
// Each render cycle:
//   1. lv_timer_handler() renders into the BACK buffer via _lvglFlush
//      (memcpy to s_gdma_fb which always points to the current back buffer).
//      No PSRAM traffic at this point — writes go only to the CPU D-cache.
//   2. Cache_WriteBack_Addr() flushes the dirty back-buffer region to PSRAM.
//      GDMA is scanning the FRONT buffer (a different PSRAM address range)
//      so there is no content corruption.  Bus contention at this moment
//      causes a brief shift of whichever front-buffer rows GDMA is scanning,
//      but those rows contain stable, unchanged content — the shift is on
//      a uniform background and is imperceptible.
//   3. requestSwap() tells the ISR to switch GDMA to the back buffer at the
//      next VSYNC_END.  The new content becomes visible one frame later
//      (~25 ms) — imperceptible for a seconds clock.
// ---------------------------------------------------------------------------
static uintptr_t s_wb_start = UINTPTR_MAX;  // start of pending writeback range
static uintptr_t s_wb_end   = 0;            // end   of pending writeback range

// ---------------------------------------------------------------------------
// Static display/indev handles
// ---------------------------------------------------------------------------
static lv_display_t   *s_display    = nullptr;
static lv_indev_t     *s_touch      = nullptr;
static DisplayManager *s_instance   = nullptr;
static uint8_t        *s_gdma_fb    = nullptr;  // LGFX PSRAM framebuffer (GDMA source)
static uint8_t        *s_render_buf = nullptr;  // Separate LVGL render buffer (PSRAM)

// ---------------------------------------------------------------------------
// VSYNC callback — called from Bus_RGB lcd_default_isr_handler() at VSYNC_END
// (injected by patch_lgfx.py).  Must be in IRAM (interrupt context).
// At VSYNC_END the GDMA has just restarted from the top of the framebuffer,
// giving ~16 ms before it reaches any typical dirty UI region.
// ---------------------------------------------------------------------------
extern "C" void IRAM_ATTR lgfx_vsync_callback() {
    s_vsync_count = s_vsync_count + 1;
    if (s_vsync_sem) {
        BaseType_t hp = pdFALSE;
        xSemaphoreGiveFromISR(s_vsync_sem, &hp);
        portYIELD_FROM_ISR(hp);
    }
}

// ---------------------------------------------------------------------------
// begin()
// ---------------------------------------------------------------------------
void DisplayManager::begin() {
    s_instance = this;

    // Initialise LovyanGFX
    _gfx.init();
    _gfx.setRotation(0);
    _gfx.setBrightness(128);   // ~50 % on startup
    _gfx.fillScreen(TFT_BLACK);

    // Initialise LVGL
    lv_init();
    // LVGL 9 uses lv_tick_set_cb() instead of the LV_TICK_CUSTOM lv_conf.h macro.
    // Without this, lv_tick_get() returns 0 → no timers fire → no display updates.
    lv_tick_set_cb([]() -> uint32_t { return (uint32_t)millis(); });

    // PARTIAL mode: LVGL renders into a separate PSRAM render buffer; the
    // flush callback byte-swaps the pixels and copies them into the GDMA
    // back buffer (s_gdma_fb).  At VSYNC the ISR swaps back→front so the
    // new content becomes visible without any mid-frame content tear.
    s_gdma_fb = _gfx.getBackBuffer();
    if (!s_gdma_fb) {
        // Fallback: Bus_RGB second-buffer alloc failed; degrade to
        // single-buffer mode (jitter possible but no crash).
        s_gdma_fb = _gfx.getFrameBuffer();
        Serial.println("[Display] WARNING: getBackBuffer() null — single-buffer fallback");
    }
    if (!s_gdma_fb) {
        Serial.println("[Display] ERROR: getFrameBuffer() returned nullptr — check use_psram=1");
        return;
    }

    // 40-row render buffer in PSRAM (40 × 800 × 2 = 64 KB)
    static constexpr int32_t RENDER_LINES = 40;
    const size_t render_size = (size_t)TFT_WIDTH * RENDER_LINES * sizeof(lv_color16_t);
    s_render_buf = static_cast<uint8_t*>(heap_caps_malloc(render_size, MALLOC_CAP_SPIRAM));
    if (!s_render_buf) {
        Serial.println("[Display] ERROR: render buffer alloc failed (PSRAM full?)");
        return;
    }
    Serial.printf("[Display] PARTIAL mode — gdma_fb=%p render_buf=%p (%u B)\n",
                  s_gdma_fb, s_render_buf, (unsigned)render_size);

    // Create VSYNC semaphore — released by lgfx_vsync_callback() in the ISR
    s_vsync_sem = xSemaphoreCreateBinary();

    // Create LVGL display with the separate render buffer (PARTIAL mode)
    s_display = lv_display_create(TFT_WIDTH, TFT_HEIGHT);
    lv_display_set_buffers(s_display, s_render_buf, nullptr, render_size,
                           LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(s_display, _lvglFlush);
    lv_display_set_user_data(s_display, this);

    // Register capacitive touch input device
    s_touch = lv_indev_create();
    lv_indev_set_type(s_touch, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(s_touch, _lvglTouch);
    lv_indev_set_user_data(s_touch, this);

    Serial.printf("[Display] ready (%d x %d), LVGL %d.%d.%d\n",
                  TFT_WIDTH, TFT_HEIGHT,
                  LVGL_VERSION_MAJOR, LVGL_VERSION_MINOR, LVGL_VERSION_PATCH);

    // Render task — high priority (5) on Core 1 (same core as the VSYNC ISR).
    // When VSYNC fires, portYIELD_FROM_ISR immediately context-switches to this
    // task.  It renders into the back buffer (CPU cache, no PSRAM traffic),
    // flushes it to PSRAM, then requests a buffer swap at the next VSYNC.
    // This double-buffering approach eliminates the mid-frame content tear
    // ("entire screen shifts right") without any timed delay.
    xTaskCreatePinnedToCore(_renderTask, "lvgl_render",
                            8192,    // stack bytes
                            this,    // arg
                            5,       // priority (> loopTask=1, < WiFi=23)
                            &_renderTaskHandle,
                            1);      // Core 1 — same core as VSYNC ISR
}

// ---------------------------------------------------------------------------
// loop()
// ---------------------------------------------------------------------------
void DisplayManager::loop() {
    // Rendering is handled by _renderTask (VSYNC-gated, high priority).
    // loop() only logs the VSYNC rate so the ISR health can be monitored.
    static uint32_t s_last_report_ms  = 0;
    static uint32_t s_last_vsync_snap = 0;
    const  uint32_t now_ms = millis();
    if (now_ms - s_last_report_ms >= 5000) {
        uint32_t delta = s_vsync_count - s_last_vsync_snap;
        Serial.printf("[Display] VSYNC/5s=%lu\n", delta);
        s_last_vsync_snap = s_vsync_count;
        s_last_report_ms  = now_ms;
    }
}

// ---------------------------------------------------------------------------
// _renderTask()  — runs at priority 5 on Core 1, same core as VSYNC ISR
// ---------------------------------------------------------------------------
void DisplayManager::_renderTask(void* /*arg*/) {
    for (;;) {
        // Block until VSYNC fires (or 50 ms fallback if ISR is silent).
        // portYIELD_FROM_ISR() in lgfx_vsync_callback() gives us an immediate
        // context switch so we start rendering within ~10 µs of VSYNC_END.
        xSemaphoreTake(s_vsync_sem, pdMS_TO_TICKS(50));

        // The ISR has applied any pending buffer swap.  Update s_gdma_fb to
        // point to the new back buffer so _lvglFlush renders there this cycle.
        s_gdma_fb = s_instance->_gfx.getBackBuffer();

        // Reset dirty range for this render cycle.
        s_wb_start = UINTPTR_MAX;
        s_wb_end   = 0;

        // Render into back buffer (CPU D-cache only — no PSRAM traffic yet).
        // _lvglFlush memcpy's into s_gdma_fb and accumulates s_wb_start/end.
        lv_timer_handler();

        if (s_wb_start < s_wb_end) {
            // Flush the dirty back-buffer region to PSRAM immediately.
            // GDMA is scanning the FRONT buffer (different PSRAM addresses) so
            // there is no content corruption — any bus contention shifts rows
            // in the unchanged front buffer, which is imperceptible.
            Cache_WriteBack_Addr((uint32_t)s_wb_start,
                                 (uint32_t)(s_wb_end - s_wb_start));
            s_wb_start = UINTPTR_MAX;
            s_wb_end   = 0;

            // Ask the ISR to swap back→front at the next VSYNC_END so the
            // freshly-rendered content becomes visible one frame later.
            s_instance->_gfx.requestSwap();
        }
    }
}

// ---------------------------------------------------------------------------
// pollTouch()  — call from main loop to update touch cache on Wire1
// ---------------------------------------------------------------------------
void DisplayManager::pollTouch() {
    uint16_t tx = 0, ty = 0;
    const bool touched = _gfx.getTouch(&tx, &ty);
    s_touch_pressed = touched;
    if (touched) {
        s_touch_x = tx;
        s_touch_y = ty;
    }
}

// ---------------------------------------------------------------------------
// setBrightness()
// ---------------------------------------------------------------------------
void DisplayManager::setBrightness(uint8_t brightness) {
    _gfx.setBrightness(brightness);
}

// ---------------------------------------------------------------------------
// showHotspotScreen()
// ---------------------------------------------------------------------------
void DisplayManager::showHotspotScreen(const char* ssid) {
    lv_lock();
    // Style the active screen background
    lv_obj_t *scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x0f0f1a), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    // Centered flex-column container (dark panel)
    lv_obj_t *cont = lv_obj_create(scr);
    lv_obj_set_size(cont, LV_PCT(70), LV_SIZE_CONTENT);
    lv_obj_center(cont);
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_color(cont, lv_color_hex(0x1a1a30), 0);
    lv_obj_set_style_border_width(cont, 0, 0);
    lv_obj_set_style_radius(cont, 12, 0);
    lv_obj_set_style_pad_all(cont, 28, 0);
    lv_obj_set_style_pad_row(cont, 12, 0);
    lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLLABLE);

    // WiFi icon
    lv_obj_t *icon = lv_label_create(cont);
    lv_label_set_text(icon, LV_SYMBOL_WIFI);
    lv_obj_set_style_text_font(icon, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(icon, lv_color_hex(0x7eb3ff), 0);

    // Title
    lv_obj_t *title = lv_label_create(cont);
    lv_label_set_text(title, "WiFi Setup");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0x7eb3ff), 0);

    // Instruction
    lv_obj_t *instr = lv_label_create(cont);
    lv_label_set_text(instr, "Connect to the WiFi hotspot:");
    lv_obj_set_style_text_font(instr, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(instr, lv_color_hex(0x8888aa), 0);

    // SSID name (highlighted)
    lv_obj_t *ssidLabel = lv_label_create(cont);
    lv_label_set_text(ssidLabel, ssid);
    lv_obj_set_style_text_font(ssidLabel, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_color(ssidLabel, lv_color_hex(0xe0e0f0), 0);

    // Sub-instruction
    lv_obj_t *sub = lv_label_create(cont);
    lv_label_set_text(sub, "then open 192.168.4.1 in your browser");
    lv_obj_set_style_text_font(sub, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(sub, lv_color_hex(0x8888aa), 0);
    lv_unlock();
}

// ---------------------------------------------------------------------------
// LVGL flush callback  (PARTIAL mode)
// ---------------------------------------------------------------------------
void DisplayManager::_lvglFlush(lv_display_t *display,
                                  const lv_area_t *area,
                                  uint8_t *px_map) {
    // px_map is the render buffer (separate from the GDMA framebuffer).
    // LVGL rendered native rgb565_t pixels into it; the GDMA framebuffer
    // needs swap565_t (byte[0]=RRRRRGGG, byte[1]=GGGBBBBB) so the LCD_CAM
    // with lcd_byte_order=0 outputs byte[0]→D[15:8] and byte[1]→D[7:0],
    // which maps correctly to the R/G/B data-bus pins of the ST7262.

    const int32_t area_w      = area->x2 - area->x1 + 1;
    const int32_t dirty_lines = area->y2 - area->y1 + 1;
    const size_t  area_px     = (size_t)area_w * dirty_lines;

    // 1) Byte-swap every pixel in the render buffer (rgb565_t → swap565_t).
    lv_draw_sw_rgb565_swap(px_map, area_px);

    // 2) Copy the swapped rows into the correct position of the GDMA framebuffer.
    //    With a full-width render buffer LVGL always renders area->x1=0,
    //    area->x2=TFT_WIDTH-1; the row-by-row loop is correct for both cases.
    uint8_t*       dst = s_gdma_fb
                       + (size_t)area->y1 * TFT_WIDTH * sizeof(lv_color16_t)
                       + (size_t)area->x1 * sizeof(lv_color16_t);
    const uint8_t* src = px_map;
    const size_t   row_bytes = (size_t)area_w * sizeof(lv_color16_t);
    for (int32_t y = 0; y < dirty_lines; y++) {
        memcpy(dst, src, row_bytes);
        dst += TFT_WIDTH * sizeof(lv_color16_t);
        src += row_bytes;
    }
    // Accumulate the dirty address range for the timed post-render flush.
    // Cache_WriteBack_Addr is NOT called here — _renderTask calls it after
    // waiting for GDMA to scan past these rows in the current frame.
    // This ensures bus accesses target non-overlapping PSRAM regions,
    // eliminating the "entire screen shifts right" jitter.
    const uintptr_t flush_start =
        (uintptr_t)(s_gdma_fb + (size_t)area->y1 * TFT_WIDTH * sizeof(lv_color16_t));
    const uintptr_t flush_end =
        flush_start + (size_t)dirty_lines * TFT_WIDTH * sizeof(lv_color16_t);
    if (flush_start < s_wb_start) s_wb_start = flush_start;
    if (flush_end   > s_wb_end)   s_wb_end   = flush_end;

    lv_display_flush_ready(display);
}

// ---------------------------------------------------------------------------
// LVGL touch read callback
// ---------------------------------------------------------------------------
void DisplayManager::_lvglTouch(lv_indev_t *indev, lv_indev_data_t *data) {
    // Read the cached touch state set by pollTouch() in the main loop.
    // Wire1 (GT911 I2C) is NOT accessed here so there is no cross-task
    // contention with SensorManager::read() on the same bus.
    data->state   = s_touch_pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
    data->point.x = static_cast<int32_t>(s_touch_x);
    data->point.y = static_cast<int32_t>(s_touch_y);
}

