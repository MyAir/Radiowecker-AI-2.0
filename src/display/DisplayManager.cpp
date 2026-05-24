#include "DisplayManager.h"
#include "../config.h"
#include "../serial_safe.h"
#include <esp_heap_caps.h>
#include <Wire.h>

// ---------------------------------------------------------------------------
// GT911 register map (subset)
// ---------------------------------------------------------------------------
static constexpr uint16_t GT911_REG_STATUS    = 0x814E; // touch count / buffer status
static constexpr uint16_t GT911_REG_POINT1    = 0x8150; // first point X_lo (8 B/point block at 0x814F)
static constexpr uint16_t GT911_REG_POINT1_ID = 0x814F; // first point track-id (start of 8 B block)
static constexpr uint8_t  GT911_BUFFER_READY  = 0x80;   // status bit7
static constexpr uint8_t  GT911_TOUCH_MASK    = 0x0F;   // status bits3:0 = # points

// ---------------------------------------------------------------------------
// Touch state cache  (written by pollTouch() in main loop, read by _lvglTouch)
// ---------------------------------------------------------------------------
static volatile bool     s_touch_pressed = false;
static volatile uint16_t s_touch_x       = 0;
static volatile uint16_t s_touch_y       = 0;
#if LOG_TOUCH
static uint16_t          s_last_log_x    = 0xFFFF;  // dedup: last logged coord
static uint16_t          s_last_log_y    = 0xFFFF;
#endif

// ---------------------------------------------------------------------------
// Diagnostic counters
// ---------------------------------------------------------------------------
static volatile uint32_t s_flush_count   = 0;
static volatile uintptr_t s_last_px_map  = 0;
static volatile int32_t  s_last_area_x1  = 0;
static volatile int32_t  s_last_area_y1  = 0;
static volatile int32_t  s_last_area_x2  = 0;
static volatile int32_t  s_last_area_y2  = 0;

// ---------------------------------------------------------------------------
// Static display/indev handles
// ---------------------------------------------------------------------------
static lv_display_t   *s_display    = nullptr;
static lv_indev_t     *s_touch      = nullptr;
static DisplayManager *s_instance   = nullptr;

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
    lv_log_register_print_cb([](lv_log_level_t /*lvl*/, const char *buf) {
        serial_safe_write(buf);
    });
    // LVGL 9 uses lv_tick_set_cb() instead of the LV_TICK_CUSTOM lv_conf.h macro.
    lv_tick_set_cb([]() -> uint32_t { return (uint32_t)millis(); });

    // -----------------------------------------------------------------
    // Rendering: LVGL PARTIAL mode with two PSRAM scratch buffers.
    //
    // This mirrors the proven approach from the Radiowecker_EEZ_AI
    // project. LVGL renders only dirty rectangles into the small scratch
    // buffer, and the flush callback pushes those pixels into the panel
    // framebuffer via gfx.writePixels(). No double-buffering of the
    // full framebuffer, no GDMA scan-source swap, no VSYNC sync — and
    // therefore no "screen shifts left to right" tear when the clock
    // updates every second.
    // -----------------------------------------------------------------
    constexpr uint32_t BUF_LINES = 100;  // 100 lines × 800 px × 2 B = 160 kB per buffer
    const size_t buf_size_bytes  = (size_t)TFT_WIDTH * BUF_LINES * sizeof(lv_color16_t);

    void* draw_buf1 = ps_malloc(buf_size_bytes);
    void* draw_buf2 = ps_malloc(buf_size_bytes);
    if (!draw_buf1 || !draw_buf2) {
        serial_safe_println("[Display] ERROR: PSRAM draw buffer allocation failed");
        return;
    }

    s_display = lv_display_create(TFT_WIDTH, TFT_HEIGHT);
    lv_display_set_buffers(s_display, draw_buf1, draw_buf2,
                           buf_size_bytes, LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(s_display, _lvglFlush);
    lv_display_set_user_data(s_display, this);

    // Register capacitive touch input device
    s_touch = lv_indev_create();
    lv_indev_set_type(s_touch, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(s_touch, _lvglTouch);
    lv_indev_set_user_data(s_touch, this);

#if LOG_DISPLAY
    serial_safe_printf("[Display] PARTIAL mode — 2× %u B PSRAM scratch buffers, %d x %d, LVGL %d.%d.%d\n",
                  (unsigned)buf_size_bytes,
                  TFT_WIDTH, TFT_HEIGHT,
                  LVGL_VERSION_MAJOR, LVGL_VERSION_MINOR, LVGL_VERSION_PATCH);
#endif
}

// ---------------------------------------------------------------------------
// loop()
// ---------------------------------------------------------------------------
void DisplayManager::loop() {
    lv_timer_handler();

#if LOG_DISPLAY
    // Diagnostic: log flush rate every 5 s
    static uint32_t s_last_report_ms  = 0;
    static uint32_t s_last_flush_snap = 0;
    const  uint32_t now_ms = millis();
    if (now_ms - s_last_report_ms >= 5000) {
        uint32_t fd = s_flush_count - s_last_flush_snap;
        serial_safe_printf(
            "[Display] flush/5s=%lu lastFlush: px=%p area=(%ld,%ld)-(%ld,%ld)\n",
            (unsigned long)fd,
            (void*)s_last_px_map,
            (long)s_last_area_x1, (long)s_last_area_y1,
            (long)s_last_area_x2, (long)s_last_area_y2);
        s_last_flush_snap = s_flush_count;
        s_last_report_ms  = now_ms;
    }
#endif
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
        // GT911 point block layout (8 B per point, starting at 0x8150):
        //   +0: track-id  +1: X_lo  +2: X_hi  +3: Y_lo  +4: Y_hi
        //   +5: size_lo   +6: size_hi  +7: reserved
        // We start the read at 0x8150 (= track-id of point 1) and stream
        // all n_points * 8 bytes in one transaction.
        const int n_bytes = n_points * 8;
        Wire1.beginTransmission(TOUCH_I2C_ADDR);
        Wire1.write((uint8_t)(GT911_REG_POINT1_ID >> 8));
        Wire1.write((uint8_t)(GT911_REG_POINT1_ID & 0xFF));
        if (Wire1.endTransmission(false) == 0 &&
            Wire1.requestFrom((int)TOUCH_I2C_ADDR, n_bytes) == n_bytes) {
            uint16_t xs[5] = {0}, ys[5] = {0}, sz[5] = {0};
            uint8_t  ids[5] = {0};
            for (uint8_t i = 0; i < n_points; i++) {
                ids[i]            = Wire1.read();
                const uint16_t xl = Wire1.read();
                const uint16_t xh = Wire1.read();
                const uint16_t yl = Wire1.read();
                const uint16_t yh = Wire1.read();
                const uint16_t sl = Wire1.read();
                const uint16_t sh = Wire1.read();
                (void)Wire1.read();                               // reserved
                xs[i] = (uint16_t)((xh << 8) | xl);
                ys[i] = (uint16_t)((yh << 8) | yl);
                sz[i] = (uint16_t)((sh << 8) | sl);
            }
            // First point feeds LVGL.
            s_touch_x = xs[0];
            s_touch_y = ys[0];
            s_touch_pressed = true;

#if LOG_TOUCH
            // Only log when coordinates change (suppress hold-still duplicates).
            if (xs[0] != s_last_log_x || ys[0] != s_last_log_y) {
                s_last_log_x = xs[0];
                s_last_log_y = ys[0];
                char line[160];
                int  off = snprintf(line, sizeof(line), "[Touch] n=%u", n_points);
                for (uint8_t i = 0; i < n_points && off < (int)sizeof(line); i++) {
                    off += snprintf(line + off, sizeof(line) - off,
                                    " p%u(id=%u x=%u y=%u sz=%u)",
                                    i, ids[i], xs[i], ys[i], sz[i]);
                }
                serial_safe_println(line);
            }
#endif
        }
    } else {
#if LOG_TOUCH
        if (s_touch_pressed) {
            serial_safe_println("[Touch] release");
            s_last_log_x = 0xFFFF;  // reset so next press always logs
            s_last_log_y = 0xFFFF;
        }
#endif
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
// showOtaScreen() — full-screen "OTA update in progress" with progress bar
// ---------------------------------------------------------------------------
void DisplayManager::showOtaScreen(const char* hostname) {
    lv_lock();

    // Clean root and reset background
    lv_obj_t *scr = lv_scr_act();
    lv_obj_clean(scr);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x0f0f1a), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    // Centered card
    lv_obj_t *cont = lv_obj_create(scr);
    lv_obj_set_size(cont, LV_PCT(75), LV_SIZE_CONTENT);
    lv_obj_center(cont);
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_color(cont, lv_color_hex(0x1a1a30), 0);
    lv_obj_set_style_border_width(cont, 0, 0);
    lv_obj_set_style_radius(cont, 12, 0);
    lv_obj_set_style_pad_all(cont, 28, 0);
    lv_obj_set_style_pad_row(cont, 14, 0);
    lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLLABLE);

    // Icon
    lv_obj_t *icon = lv_label_create(cont);
    lv_label_set_text(icon, LV_SYMBOL_DOWNLOAD);
    lv_obj_set_style_text_font(icon, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(icon, lv_color_hex(0x7eb3ff), 0);

    // Title
    lv_obj_t *title = lv_label_create(cont);
    lv_label_set_text(title, "Firmware Update");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0x7eb3ff), 0);

    // Hostname / source
    lv_obj_t *host = lv_label_create(cont);
    lv_label_set_text_fmt(host, "Receiving update for %s", hostname ? hostname : "device");
    lv_obj_set_style_text_font(host, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(host, lv_color_hex(0x8888aa), 0);

    // Reset last-rendered percent so the next updateOtaProgress(0) draws.
    _otaLastPct = 0xFF;

    // Progress bar
    _otaBar = lv_bar_create(cont);
    lv_obj_set_size(_otaBar, LV_PCT(95), 24);
    lv_bar_set_range(_otaBar, 0, 100);
    lv_bar_set_value(_otaBar, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(_otaBar, lv_color_hex(0x0f0f1a), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(_otaBar, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_color(_otaBar, lv_color_hex(0x333355), LV_PART_MAIN);
    lv_obj_set_style_border_width(_otaBar, 1, LV_PART_MAIN);
    lv_obj_set_style_radius(_otaBar, 6, LV_PART_MAIN);
    lv_obj_set_style_bg_color(_otaBar, lv_color_hex(0x3a7bd5), LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(_otaBar, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_radius(_otaBar, 6, LV_PART_INDICATOR);

    // Percent label
    _otaPctLbl = lv_label_create(cont);
    lv_label_set_text(_otaPctLbl, "0 %");
    lv_obj_set_style_text_font(_otaPctLbl, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(_otaPctLbl, lv_color_hex(0xe0e0f0), 0);

    // Status / hint
    _otaStatus = lv_label_create(cont);
    lv_label_set_text(_otaStatus, "Do not power off the device.");
    lv_obj_set_style_text_font(_otaStatus, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(_otaStatus, lv_color_hex(0x8888aa), 0);

    lv_unlock();

    // Force first frame so the screen is on display before transfer begins.
    tick();
}

// ---------------------------------------------------------------------------
// updateOtaProgress()
// ---------------------------------------------------------------------------
void DisplayManager::updateOtaProgress(uint8_t percent) {
    if (percent > 100) percent = 100;
    if (_otaBar == nullptr) return;
    // ArduinoOTA fires onProgress per packet (~1500 times for a 1.4 MB image).
    // Repainting that often on the ST7262 RGB panel — which has no VSYNC and
    // a continuously scanned framebuffer — produces a left-right tear band as
    // the dirty rectangle marches across the indicator. Repaint only when the
    // integer percent actually advances.
    if (percent == _otaLastPct) return;
    _otaLastPct = percent;
    lv_lock();
    lv_bar_set_value(_otaBar, percent, LV_ANIM_OFF);
    if (_otaPctLbl) lv_label_set_text_fmt(_otaPctLbl, "%u %%", (unsigned)percent);
    lv_unlock();
    tick();
}

// ---------------------------------------------------------------------------
// showOtaError()
// ---------------------------------------------------------------------------
void DisplayManager::showOtaError(const char* msg) {
    if (_otaStatus == nullptr) return;
    lv_lock();
    lv_label_set_text_fmt(_otaStatus, "Update failed: %s", msg ? msg : "unknown");
    lv_obj_set_style_text_color(_otaStatus, lv_color_hex(0xff6666), 0);
    if (_otaPctLbl) {
        lv_obj_set_style_text_color(_otaPctLbl, lv_color_hex(0xff6666), 0);
    }
    lv_unlock();
    tick();
}

// ---------------------------------------------------------------------------
// tick() — service LVGL once (used during blocking OTA transfer)
// ---------------------------------------------------------------------------
void DisplayManager::tick() {
    lv_timer_handler();
}

// ---------------------------------------------------------------------------
// LVGL flush callback  (PARTIAL mode)
//
// LVGL renders dirty rectangles into a small PSRAM scratch buffer.  We push
// those pixels into the panel framebuffer via gfx.writePixels() — exactly
// the approach used by the working Radiowecker_EEZ_AI project.  No buffer
// swap, no VSYNC sync, no manual cache writeback: LovyanGFX takes care of
// the byte order and PSRAM coherency, and the dirty regions are small
// enough that any tearing is invisible.
// ---------------------------------------------------------------------------
void DisplayManager::_lvglFlush(lv_display_t *display,
                                  const lv_area_t *area,
                                  uint8_t *px_map) {
    const int32_t w = area->x2 - area->x1 + 1;
    const int32_t h = area->y2 - area->y1 + 1;

    // Push the dirty rectangle to the panel framebuffer using the same
    // sequence the working Radiowecker_EEZ_AI project uses. With
    // LV_COLOR_16_SWAP=1 the px_map bytes are already in the panel's
    // expected order, so writePixels can DMA them straight in.
    auto &gfx = s_instance->_gfx;
    gfx.startWrite();
    gfx.setAddrWindow(area->x1, area->y1, w, h);
    gfx.writePixels((uint16_t*)px_map, (uint32_t)w * (uint32_t)h);
    gfx.endWrite();

    s_flush_count  = s_flush_count + 1;
    s_last_px_map  = (uintptr_t)px_map;
    s_last_area_x1 = area->x1;
    s_last_area_y1 = area->y1;
    s_last_area_x2 = area->x2;
    s_last_area_y2 = area->y2;

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

