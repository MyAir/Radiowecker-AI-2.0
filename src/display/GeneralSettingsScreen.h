#pragma once
#include <Arduino.h>
#include <functional>
#include <lvgl.h>

/**
 * GeneralSettingsScreen
 *
 * Single-screen child of SettingsScreen. Currently exposes one setting:
 *   "Schlummerdauer (Min.)" — snooze duration 1..30
 *
 * Persists via g_appConfig.setSnoozeMinutes() (writes /config.json on SD).
 *
 * Lifecycle mirrors AlarmSetupScreen:
 *   - create(mainScr) slides in over the (already-detached) settings screen
 *     and lets LVGL auto-delete it after the animation.
 *   - Back button slides back to mainScr and deletes this screen.
 *   - 30 s inactivity timer auto-returns.
 */
class GeneralSettingsScreen {
public:
    using SimpleCallback     = std::function<void()>;
    using BrightnessCallback = std::function<void(uint8_t)>;

    /** Build & show. */
    void create(lv_obj_t* mainScr, uint8_t currentBrightness);

    void setOnBrightnessChange(BrightnessCallback cb) { _onBrightness = cb; }

private:
    lv_obj_t*   _scr     = nullptr;
    lv_obj_t*   _mainScr = nullptr;
    lv_obj_t*   _spinSnooze = nullptr;
    lv_obj_t*   _brightnessSlider = nullptr;
    lv_timer_t* _timer   = nullptr;

    BrightnessCallback _onBrightness = nullptr;

    static constexpr uint32_t TIMEOUT_MS = 30000;

    void _goBack();
    static void _backBtnCb(lv_event_t* e);
    static void _spinDecCb(lv_event_t* e);
    static void _spinIncCb(lv_event_t* e);
    static void _spinValueCb(lv_event_t* e);
    static void _brightnessCb(lv_event_t* e);
    static void _timeoutCb(lv_timer_t* t);
    void        _resetTimer();
};

extern GeneralSettingsScreen generalSettingsScreen;
