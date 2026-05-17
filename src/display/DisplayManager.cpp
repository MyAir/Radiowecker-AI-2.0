#include "DisplayManager.h"
#include "../config.h"
#include <esp_cache.h>
#include <esp_heap_caps.h>
#include <freertos/semphr.h>

// ---------------------------------------------------------------------------
// VSYNC semaphore — given by lgfx_vsync_callback() at every VSYNC_END ISR.
// DisplayManager::loop() takes it (non-blocking) before calling lv_refr_now().
// This synchronises rendering to the DMA scan start, ensuring the cache flush
// completes before the GDMA reaches the just-rendered dirty region.
// ---------------------------------------------------------------------------
static SemaphoreHandle_t s_vsync_sem = nullptr;

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

    // PARTIAL mode: LVGL renders into a separate PSRAM render buffer; the
    // flush callback byte-swaps the pixels and copies them into the GDMA
    // framebuffer.  This avoids the read-back feedback loop that occurs in
    // DIRECT mode (LVGL reads swap565_t bytes as rgb565_t → wrong blending
    // → swap again → 2-state oscillation).
    s_gdma_fb = _gfx.getFrameBuffer();
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
}

// ---------------------------------------------------------------------------
// loop()
// ---------------------------------------------------------------------------
void DisplayManager::loop() {
    lv_timer_handler_run_in_period(5);

    // Render only when the VSYNC ISR fires (non-blocking semaphore take).
    // At that moment the GDMA has just restarted from the top of the
    // framebuffer, so rendering + cache flush completes long before the DMA
    // scan reaches any typical dirty region (clock at y~130 ≈ 4 ms away).
    // This eliminates the cache/DMA race that caused garbled text and jitter.
    if (s_vsync_sem && xSemaphoreTake(s_vsync_sem, 0) == pdTRUE) {
        lv_refr_now(s_display);
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

    // 3) Writeback CPU cache → PSRAM for the modified GDMA framebuffer rows.
    //    esp_cache_msync requires 32-byte-aligned address and size.
    uint8_t* const fb_row = s_gdma_fb
                          + (size_t)area->y1 * TFT_WIDTH * sizeof(lv_color16_t);
    const size_t   wb_bytes = (size_t)dirty_lines * TFT_WIDTH * sizeof(lv_color16_t);
    const uintptr_t start = (uintptr_t)fb_row & ~(uintptr_t)31;
    const uintptr_t end   = ((uintptr_t)fb_row + wb_bytes + 31) & ~(uintptr_t)31;
    esp_cache_msync((void*)start, end - start,
                    ESP_CACHE_MSYNC_FLAG_DIR_C2M |
                    ESP_CACHE_MSYNC_FLAG_TYPE_DATA);

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

