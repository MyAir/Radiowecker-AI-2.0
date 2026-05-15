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
        Serial.println("[Display] PSRAM alloc failed — falling back to internal RAM");
        free(s_buf1);
        s_buf1 = new lv_color16_t[TFT_WIDTH * 10];
        s_buf2 = new lv_color16_t[TFT_WIDTH * 10];
    }

    // Create LVGL display
    s_display = lv_display_create(TFT_WIDTH, TFT_HEIGHT);
    lv_display_set_buffers(s_display, s_buf1, s_buf2, BUF_SIZE,
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
}

// ---------------------------------------------------------------------------
// loop()
// ---------------------------------------------------------------------------
void DisplayManager::loop() {
    lv_timer_handler();
}

// ---------------------------------------------------------------------------
// setBrightness()
// ---------------------------------------------------------------------------
void DisplayManager::setBrightness(uint8_t brightness) {
    _gfx.setBrightness(brightness);
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

    dm->_gfx.startWrite();
    dm->_gfx.setAddrWindow(area->x1, area->y1, w, h);
    dm->_gfx.writePixels(reinterpret_cast<lgfx::rgb565_t*>(px_map), w * h);
    dm->_gfx.endWrite();

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

