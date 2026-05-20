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

    /** Read the GT911 touch state and cache it for the LVGL indev callback.
     *  Call this from the main loop so that Wire1 (shared with sensors)
     *  is only accessed from a single task, avoiding ESP_ERR_INVALID_STATE.
     */
    void pollTouch();

    /** Set backlight brightness 0..255 (255 = full on). */
    void setBrightness(uint8_t brightness);

    /**
     * Show a full-screen WiFi setup message when the captive-portal AP is active.
     * @param ssid  The hotspot SSID to display (e.g. WIFI_AP_SSID from config.h).
     */
    void showHotspotScreen(const char* ssid);

    /**
     * Show a full-screen "OTA update in progress" screen with a progress bar.
     * Replaces the active screen until reboot.
     */
    void showOtaScreen(const char* hostname);

    /** Update the OTA progress bar (0..100). Safe to call from OTA callback. */
    void updateOtaProgress(uint8_t percent);

    /** Show an OTA error message on the OTA screen. */
    void showOtaError(const char* msg);

    /** Render a single LVGL frame immediately (used during OTA to refresh UI). */
    void tick();

    /** Expose the LGFX instance for direct drawing if needed. */
    LGFX& gfx() { return _gfx; }

private:
    LGFX  _gfx;

    // OTA screen widgets (created lazily by showOtaScreen)
    lv_obj_t* _otaBar     = nullptr;
    lv_obj_t* _otaPctLbl  = nullptr;
    lv_obj_t* _otaStatus  = nullptr;

    /** Initialise GT911 over Wire1 (reset pulse + clear status register). */
    void _initGT911();

    /** LVGL flush callback — copies rendered pixels to the panel. */
    static void _lvglFlush(lv_display_t *display,
                            const lv_area_t *area,
                            uint8_t *px_map);

    /** LVGL touch read callback. */
    static void _lvglTouch(lv_indev_t *indev, lv_indev_data_t *data);
};
