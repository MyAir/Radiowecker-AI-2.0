#include "DisplayManager.h"
#include "../config.h"
#include "../serial_safe.h"
#include <esp_heap_caps.h>
#include <freertos/semphr.h>
#include <rom/cache.h>
#include <Wire.h>

// ---------------------------------------------------------------------------
// GT911 register map (subset)
// ---------------------------------------------------------------------------
static constexpr uint16_t GT911_REG_STATUS    = 0x814E; // touch count / buffer status
static constexpr uint16_t GT911_REG_POINT1    = 0x8150; // first point block (8 B)
static constexpr uint8_t  GT911_BUFFER_READY  = 0x80;   // status bit7
static constexpr uint8_t  GT911_TOUCH_MASK    = 0x0F;   // status bits3:0 = # points

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
// Diagnostic counters — written by _lvglFlush (render task) and read by loop()
// (main task) every 5 s.  uint32 writes are atomic on ESP32-S3 so no mutex
// is needed for monotonic counters; for the last-flush snapshot we accept
// the rare race where the snapshot tears (it's only diagnostic).
// ---------------------------------------------------------------------------
static volatile uint32_t s_flush_count       = 0; // total flush_cb invocations
static volatile uint32_t s_swap_count        = 0; // total requestSwap() calls
static volatile uint32_t s_flushes_in_refr   = 0; // running count, reset on is_last
static volatile uint32_t s_last_flushes_per_refr = 0;
static volatile uintptr_t s_last_px_map      = 0;
static volatile int32_t  s_last_area_x1      = 0;
static volatile int32_t  s_last_area_y1      = 0;
static volatile int32_t  s_last_area_x2      = 0;
static volatile int32_t  s_last_area_y2      = 0;

// ---------------------------------------------------------------------------
// Rendering architecture — LVGL DIRECT mode with two PSRAM framebuffers.
//
// Bus_RGB allocates two PSRAM framebuffers (_frame_buffer + _frame_buffer2).
// At every VSYNC_END the ISR swaps which one GDMA scans if requestSwap()
// was called.
//
// We hand BOTH framebuffers to LVGL via lv_display_set_buffers() with
// LV_DISPLAY_RENDER_MODE_DIRECT.  LVGL then:
//   * alternates between the two buffers each refresh,
//   * automatically re-renders the PREVIOUS frame's dirty rectangles into
//     the now-current buffer (so the buffer never "falls behind"),
//   * renders this frame's dirty rectangles on top.
//
// Result: both buffers stay perfectly coherent without any manual front→back
// memcpy and without the ISR-race window of the old PARTIAL+sync scheme.
//
// Each render cycle:
//   1. _renderTask blocks on VSYNC sem.  Right after VSYNC_END the ISR has
//      applied any pending swap; we wake with ~10 µs latency.
//   2. lv_timer_handler() lets LVGL render dirty rects into the BACK buffer
//      (the one LVGL knows is "current", which corresponds to GDMA's back).
//   3. _lvglFlush byte-swaps the dirty pixels in place and writes the dirty
//      cache lines back to PSRAM.
//   4. On the last flush of the frame, requestSwap() tells the ISR to flip
//      GDMA to the back buffer at the next VSYNC_END.  The new content
//      becomes visible one frame later (~25 ms).
//
// LVGL’s buffer alternation and GDMA’s swap stay in lockstep because:
//   * LVGL only advances its active-buffer index when a real flush happens.
//   * We only requestSwap() when a real flush happens.
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// Static display/indev handles
// ---------------------------------------------------------------------------
static lv_display_t   *s_display    = nullptr;
static lv_indev_t     *s_touch      = nullptr;
static DisplayManager *s_instance   = nullptr;

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

    // Bring the GT911 out of reset.  Wire1 is initialised later by
    // SensorManager::begin(); the first pollTouch() call will perform the
    // actual I2C transactions once the bus is up.
    _initGT911();

    // Initialise LVGL
    lv_init();
    // Route LVGL log output through the mutex-protected Serial helper so
    // render-task warnings never race with main-task prints (was causing
    // first chars of "[Sensors]" / "[Display]" lines to be dropped).
    lv_log_register_print_cb([](lv_log_level_t /*lvl*/, const char *buf) {
        serial_safe_write(buf);
    });
    // LVGL 9 uses lv_tick_set_cb() instead of the LV_TICK_CUSTOM lv_conf.h macro.
    // Without this, lv_tick_get() returns 0 → no timers fire → no display updates.
    lv_tick_set_cb([]() -> uint32_t { return (uint32_t)millis(); });

    // DIRECT mode: LVGL gets both PSRAM framebuffers and renders dirty
    // rectangles directly into them, alternating each refresh.  LVGL also
    // re-renders the previous frame's dirty rects into the current buffer
    // so the two buffers never diverge.
    //
    // Initial state (before any swap): GDMA scans _frame_buffer, so we hand
    // LVGL fb_back=_frame_buffer2 first — LVGL writes there while GDMA is
    // still scanning _frame_buffer, then we requestSwap() to flip.
    uint8_t* fb_front_initial = _gfx.getFrontBuffer();  // == _frame_buffer
    uint8_t* fb_back_initial  = _gfx.getBackBuffer();   // == _frame_buffer2
    if (!fb_front_initial || !fb_back_initial) {
        serial_safe_println("[Display] ERROR: getFrontBuffer/getBackBuffer null — check use_psram=1 and Patch 7");
        return;
    }
    const size_t fb_size = (size_t)TFT_WIDTH * TFT_HEIGHT * sizeof(lv_color16_t);
    serial_safe_printf("[Display] DIRECT mode — buf1=%p buf2=%p (%u B each)\n",
                  fb_back_initial, fb_front_initial, (unsigned)fb_size);

    // Create VSYNC semaphore — released by lgfx_vsync_callback() in the ISR
    s_vsync_sem = xSemaphoreCreateBinary();

    // Create LVGL display with both framebuffers in DIRECT mode.
    // LVGL writes into buf1 first — we pass fb_back_initial as buf1 so the
    // first render lands in the buffer GDMA is NOT scanning.
    s_display = lv_display_create(TFT_WIDTH, TFT_HEIGHT);
    lv_display_set_buffers(s_display, fb_back_initial, fb_front_initial,
                           fb_size, LV_DISPLAY_RENDER_MODE_DIRECT);
    lv_display_set_flush_cb(s_display, _lvglFlush);
    lv_display_set_user_data(s_display, this);

    // Register capacitive touch input device
    s_touch = lv_indev_create();
    lv_indev_set_type(s_touch, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(s_touch, _lvglTouch);
    lv_indev_set_user_data(s_touch, this);

    serial_safe_printf("[Display] ready (%d x %d), LVGL %d.%d.%d\n",
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
    static uint32_t s_last_flush_snap = 0;
    static uint32_t s_last_swap_snap  = 0;
    const  uint32_t now_ms = millis();
    if (now_ms - s_last_report_ms >= 5000) {
        uint32_t vd = s_vsync_count - s_last_vsync_snap;
        uint32_t fd = s_flush_count - s_last_flush_snap;
        uint32_t sd = s_swap_count  - s_last_swap_snap;
        serial_safe_printf(
            "[Display] VSYNC/5s=%lu flush/5s=%lu swap/5s=%lu lastFlush: px=%p area=(%ld,%ld)-(%ld,%ld) flushesInRefr=%lu\n",
            (unsigned long)vd, (unsigned long)fd, (unsigned long)sd,
            (void*)s_last_px_map,
            (long)s_last_area_x1, (long)s_last_area_y1,
            (long)s_last_area_x2, (long)s_last_area_y2,
            (unsigned long)s_last_flushes_per_refr);
        s_last_vsync_snap = s_vsync_count;
        s_last_flush_snap = s_flush_count;
        s_last_swap_snap  = s_swap_count;
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
        // At this point the ISR has already applied any pending buffer swap,
        // so LVGL’s notion of "current buffer" is in lockstep with GDMA’s
        // back buffer.
        xSemaphoreTake(s_vsync_sem, pdMS_TO_TICKS(50));
        lv_timer_handler();
    }
}

// ---------------------------------------------------------------------------
// pollTouch()  — manual GT911 polling over Wire1
// ---------------------------------------------------------------------------
void DisplayManager::pollTouch() {
    // Read status register at 0x814E.
    Wire1.beginTransmission(TOUCH_I2C_ADDR);
    Wire1.write((uint8_t)(GT911_REG_STATUS >> 8));
    Wire1.write((uint8_t)(GT911_REG_STATUS & 0xFF));
    if (Wire1.endTransmission(false) != 0) return;
    if (Wire1.requestFrom((int)TOUCH_I2C_ADDR, 1) != 1) return;
    const uint8_t status = Wire1.read();

    // Only act on a frame the controller has finished writing.
    if (!(status & GT911_BUFFER_READY)) {
        s_touch_pressed = false;
        return;
    }

    const uint8_t n_points = status & GT911_TOUCH_MASK;
    if (n_points >= 1 && n_points <= 5) {
        // GT911 point-1 block (track-id is at 0x814F, we start at 0x8150):
        //   0x8150: X_lo  0x8151: X_hi  0x8152: Y_lo  0x8153: Y_hi
        //   0x8154: size_lo  0x8155: size_hi
        Wire1.beginTransmission(TOUCH_I2C_ADDR);
        Wire1.write((uint8_t)(GT911_REG_POINT1 >> 8));
        Wire1.write((uint8_t)(GT911_REG_POINT1 & 0xFF));
        if (Wire1.endTransmission(false) == 0 &&
            Wire1.requestFrom((int)TOUCH_I2C_ADDR, 6) == 6) {
            const uint16_t x_lo = Wire1.read();
            const uint16_t x_hi = Wire1.read();
            const uint16_t y_lo = Wire1.read();
            const uint16_t y_hi = Wire1.read();
            (void)Wire1.read(); (void)Wire1.read();               // size lo/hi
            s_touch_x = (uint16_t)((x_hi << 8) | x_lo);
            s_touch_y = (uint16_t)((y_hi << 8) | y_lo);
            s_touch_pressed = true;
        }
    } else {
        s_touch_pressed = false;
    }

    // Clear status register so the controller can write the next frame.
    Wire1.beginTransmission(TOUCH_I2C_ADDR);
    Wire1.write((uint8_t)(GT911_REG_STATUS >> 8));
    Wire1.write((uint8_t)(GT911_REG_STATUS & 0xFF));
    Wire1.write((uint8_t)0);
    Wire1.endTransmission();
}

// ---------------------------------------------------------------------------
// _initGT911()  — RST pulse to select I2C address, then clear status.
// Wire1 is initialised by SensorManager::begin() which runs AFTER
// display.begin(), so we do not call Wire1.begin() here.  We only drive RST.
// With INT floating (TOUCH_INT_PIN == -1) the GT911 selects address 0x5D.
// ---------------------------------------------------------------------------
void DisplayManager::_initGT911() {
    if (TOUCH_RST_PIN < 0) return;
    pinMode(TOUCH_RST_PIN, OUTPUT);
    digitalWrite(TOUCH_RST_PIN, LOW);
    delay(10);
    digitalWrite(TOUCH_RST_PIN, HIGH);
    delay(60);    // GT911 boot-up time
    // Actual I2C clear of the status register happens on first pollTouch()
    // once SensorManager::begin() has initialised Wire1.
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
    // DIRECT mode: px_map points to the START of the framebuffer LVGL just
    // rendered into (one of the two PSRAM framebuffers).  LVGL only touched
    // the pixels inside `area`; the rest of the buffer is whatever LVGL
    // copied/preserved from the previous frame.
    //
    // The ST7262 panel via Bus_RGB (lcd_byte_order=0) wants swap565_t in
    // PSRAM (byte[0]=RRRRRGGG, byte[1]=GGGBBBBB).  LVGL writes native
    // rgb565_t, so we byte-swap the dirty rectangle row-by-row.

    const int32_t area_w        = area->x2 - area->x1 + 1;
    const int32_t dirty_lines   = area->y2 - area->y1 + 1;
    const size_t  row_pitch     = (size_t)TFT_WIDTH * sizeof(lv_color16_t);
    const size_t  col_off_bytes = (size_t)area->x1 * sizeof(lv_color16_t);

    // 1) Byte-swap the dirty pixels in place (per row, only the dirty cols).
    uint8_t* row = px_map + (size_t)area->y1 * row_pitch + col_off_bytes;
    for (int32_t y = 0; y < dirty_lines; y++) {
        lv_draw_sw_rgb565_swap(row, (size_t)area_w);
        row += row_pitch;
    }

    // 2) Push the dirty rows from CPU D-cache to PSRAM (32-byte aligned).
    //    GDMA is scanning the OTHER framebuffer (LVGL guarantees we wrote
    //    into the back buffer), so this writeback never races GDMA reads.
    uint8_t* dirty_start = px_map + (size_t)area->y1 * row_pitch;
    const size_t dirty_len = (size_t)dirty_lines * row_pitch;
    const uintptr_t cstart =  (uintptr_t)dirty_start              & ~(uintptr_t)31;
    const uintptr_t cend   = ((uintptr_t)(dirty_start + dirty_len) + 31) & ~(uintptr_t)31;
    Cache_WriteBack_Addr((uint32_t)cstart, (uint32_t)(cend - cstart));

    // 3) On the last flush of this refresh, ask the ISR to swap back→front
    //    at the next VSYNC_END so the freshly-rendered content becomes
    //    visible one frame later.  Multiple flushes per refresh are coalesced
    //    into a single swap by gating on lv_display_flush_is_last().
    s_flush_count     = s_flush_count + 1;
    s_flushes_in_refr = s_flushes_in_refr + 1;
    s_last_px_map     = (uintptr_t)px_map;
    s_last_area_x1    = area->x1;
    s_last_area_y1    = area->y1;
    s_last_area_x2    = area->x2;
    s_last_area_y2    = area->y2;
    if (lv_display_flush_is_last(display)) {
        s_last_flushes_per_refr = s_flushes_in_refr;
        s_flushes_in_refr = 0;
        s_swap_count = s_swap_count + 1;
        s_instance->_gfx.requestSwap();
    }

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

