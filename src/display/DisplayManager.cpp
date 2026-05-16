#include "DisplayManager.h"
#include "../config.h"
#include <esp_heap_caps.h>

// ---------------------------------------------------------------------------
// LVGL draw buffers — allocated in PSRAM for maximum size
// 800 * 50 lines * 2 bytes/pixel = 80 KB per buffer
// ---------------------------------------------------------------------------
static constexpr size_t BUF_LINES = 50;
static constexpr size_t BUF_SIZE  = TFT_WIDTH * BUF_LINES * sizeof(lv_color16_t);

static lv_color16_t *s_buf1 = nullptr;
static lv_color16_t *s_buf2 = nullptr;

// ---------------------------------------------------------------------------
// Static display/indev handles
// ---------------------------------------------------------------------------
static lv_display_t *s_display = nullptr;
static lv_indev_t   *s_touch   = nullptr;
static DisplayManager *s_instance = nullptr;

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

    // Allocate render buffers in PSRAM
    s_buf1 = static_cast<lv_color16_t*>(
        heap_caps_malloc(BUF_SIZE, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    s_buf2 = static_cast<lv_color16_t*>(
        heap_caps_malloc(BUF_SIZE, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));

    if (!s_buf1 || !s_buf2) {
        Serial.println("[Display] PSRAM alloc FAILED — falling back to internal RAM");
        free(s_buf1);
        free(s_buf2);
        // Fallback: allocate 10 lines in internal RAM and use the matching size
        static constexpr size_t FALLBACK_LINES = 10;
        static constexpr size_t FALLBACK_SIZE  = TFT_WIDTH * FALLBACK_LINES * sizeof(lv_color16_t);
        s_buf1 = new lv_color16_t[TFT_WIDTH * FALLBACK_LINES];
        s_buf2 = new lv_color16_t[TFT_WIDTH * FALLBACK_LINES];
        Serial.printf("[Display] Fallback buf1=%p buf2=%p size=%u\n",
                      s_buf1, s_buf2, (unsigned)FALLBACK_SIZE);
        s_display = lv_display_create(TFT_WIDTH, TFT_HEIGHT);
        lv_display_set_buffers(s_display, s_buf1, s_buf2, FALLBACK_SIZE,
                               LV_DISPLAY_RENDER_MODE_PARTIAL);
    } else {
        Serial.printf("[Display] PSRAM OK — buf1=%p buf2=%p BUF_SIZE=%u\n",
                      s_buf1, s_buf2, (unsigned)BUF_SIZE);
        // Create LVGL display
        s_display = lv_display_create(TFT_WIDTH, TFT_HEIGHT);
        lv_display_set_buffers(s_display, s_buf1, s_buf2, BUF_SIZE,
                               LV_DISPLAY_RENDER_MODE_PARTIAL);
    }
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
}

// ---------------------------------------------------------------------------
// loop()
// ---------------------------------------------------------------------------
void DisplayManager::loop() {
    lv_timer_handler_run_in_period(5);
    // lv_timer_resume() only clears the pause flag; it does NOT reset last_run,
    // so the refr_timer still waits its full 10 ms period before the handler
    // picks it up.  lv_refr_now() bypasses that by calling lv_display_refr_timer()
    // directly.  It is a no-op when inv_p == 0 (nothing dirty), so calling it
    // every loop iteration costs only the update-layout traversal, not a flush.
    lv_refr_now(s_display);
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
}

// ---------------------------------------------------------------------------
// LVGL flush callback
// ---------------------------------------------------------------------------
void DisplayManager::_lvglFlush(lv_display_t *display,
                                  const lv_area_t *area,
                                  uint8_t *px_map) {
    auto *dm = static_cast<DisplayManager*>(lv_display_get_user_data(display));
    const int32_t w = area->x2 - area->x1 + 1;
    const int32_t h = area->y2 - area->y1 + 1;

    // Batch all strips of a render cycle under ONE LGFX transaction so that
    // Cache_WriteBack_Addr fires exactly once (on the final endWrite) instead
    // of once per strip.  Multiple writebacks racing with the LCD_CAM DMA
    // reading the same PSRAM frame buffer are the root cause of the horizontal
    // jitter / tearing seen at the clock-update frequency (1 Hz).
    //
    // LGFX _start_count reference counting:
    //   outer startWrite            → count = 1
    //   setAddrWindow inner pair    → 2 then back to 1  (no writeback)
    //   writePixels   inner pair    → 2 then back to 1  (no writeback)
    //   outer endWrite (last strip) → count = 0 → Cache_WriteBack_Addr ×1
    static bool s_flushOpen = false;
    if (!s_flushOpen) {
        dm->_gfx.startWrite();
        s_flushOpen = true;
    }

    dm->_gfx.setAddrWindow(area->x1, area->y1, w, h);
    dm->_gfx.writePixels(reinterpret_cast<lgfx::rgb565_t*>(px_map), w * h);

    if (lv_display_flush_is_last(display)) {
        dm->_gfx.endWrite();
        s_flushOpen = false;
    }

    lv_display_flush_ready(display);
}

// ---------------------------------------------------------------------------
// LVGL touch read callback
// ---------------------------------------------------------------------------
void DisplayManager::_lvglTouch(lv_indev_t *indev, lv_indev_data_t *data) {
    auto *dm = static_cast<DisplayManager*>(lv_indev_get_user_data(indev));
    uint16_t tx = 0, ty = 0;
    const bool touched = dm->_gfx.getTouch(&tx, &ty);
    data->state   = touched ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
    data->point.x = static_cast<int32_t>(tx);
    data->point.y = static_cast<int32_t>(ty);
}

