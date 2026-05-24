#pragma once
#include <Arduino.h>
#include <lvgl.h>

/**
 * SettingsScreen
 *
 * Slides in over the main screen from the left.  Contains:
 *   - Working back button (top-left)  → returns to main screen
 *   - Mock-up buttons: "General Settings", "Alarms"
 *   - Working audio buttons: Play SD MP3, Play SRF 3, Stop
 *   - Volume slider
 *   - 30 s inactivity timeout → auto-returns to main screen
 *
 * Call create() from within an LVGL event callback (no lv_lock() needed).
 * Wire callbacks before calling create().
 */
class SettingsScreen {
public:
    using Callback          = void(*)();
    using VolumeCallback     = void(*)(uint8_t vol);
    using BrightnessCallback = void(*)(uint8_t brightness);

    /** Build the settings LVGL screen and slide it in over mainScr. */
    void create(lv_obj_t* mainScr, uint8_t currentVolume, uint8_t currentBrightness);

    void setOnPlaySD(Callback cb)                   { _onPlaySD     = cb; }
    void setOnPlaySRF3(Callback cb)                  { _onPlaySRF3   = cb; }
    void setOnStop(Callback cb)                      { _onStop       = cb; }
    void setOnVolumeChange(VolumeCallback cb)         { _onVolume     = cb; }
    void setOnBrightnessChange(BrightnessCallback cb) { _onBrightness = cb; }

private:
    lv_obj_t*          _scr              = nullptr;
    lv_obj_t*          _mainScr          = nullptr;
    lv_obj_t*          _slider           = nullptr;
    lv_obj_t*          _brightnessSlider = nullptr;
    lv_timer_t*        _timer            = nullptr;

    Callback           _onPlaySD     = nullptr;
    Callback           _onPlaySRF3   = nullptr;
    Callback           _onStop       = nullptr;
    VolumeCallback     _onVolume     = nullptr;
    BrightnessCallback _onBrightness = nullptr;

    /** Load main screen, delete this screen after animation. */
    void _goBack();

    // Timer callback
    static void _timeoutCb(lv_timer_t* t);

    // Button callbacks
    static void _backBtnCb(lv_event_t* e);
    static void _mockBtnCb(lv_event_t* e);   // timer-reset only
    static void _playSDCb(lv_event_t* e);
    static void _playSRF3Cb(lv_event_t* e);
    static void _stopCb(lv_event_t* e);
    static void _volumeCb(lv_event_t* e);
    static void _brightnessCb(lv_event_t* e);

    /** Create a styled button. Returns the button object. */
    lv_obj_t* _makeBtn(lv_obj_t* parent, const char* label,
                        int x, int y, int w, int h,
                        uint32_t textColor, bool dimBorder = false);
};
