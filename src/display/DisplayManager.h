#pragma once
#include <Arduino.h>
#include <time.h>
#include <lvgl.h>
#include "lgfx_config.h"

/**
 * DisplayManager
 *
 * Owns the LGFX driver (LovyanGFX) and the LVGL runtime.
 * Call begin() once in setup(), then loop() every iteration.
 *
 * The actual screen content is built by EEZ Studio–generated UI code
 * placed under /lib/ui/. DisplayManager only initialises the
 * driver stack and provides a brightness API.
 */
class DisplayManager {
public:
    /**
     * Initialise LGFX, LVGL, frame buffers, flush/touch callbacks.
     * Must be called before any lv_* API.
     */
    void begin();

    /**
     * Must be called from loop() — drives the LVGL timer engine.
     */
    void loop();

    /** Set backlight brightness 0..255 (255 = full on). */
    void setBrightness(uint8_t brightness);

    /** Expose the LGFX instance for direct drawing if needed. */
    LGFX& gfx() { return _gfx; }

private:
    LGFX  _gfx;

    /** LVGL flush callback — copies rendered pixels to the panel. */
    static void _lvglFlush(lv_display_t *display,
                            const lv_area_t *area,
                            uint8_t *px_map);

    /** LVGL touch read callback. */
    static void _lvglTouch(lv_indev_t *indev, lv_indev_data_t *data);
};
