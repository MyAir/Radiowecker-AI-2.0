#pragma once
#include <Arduino.h>
#include <functional>
#include <lvgl.h>

/**
 * GeneralSettingsScreen — "System" tab inside SettingsScreen's tabview.
 *
 * Content (top-down):
 *   - Snooze duration (1..30 min)
 *   - Max. alarm duration (0..60 min, 0 = no limit)
 *   - Inactivity timeout (0..300 s, 0 = no auto-close)
 *   - Brightness Main / Alarm / Settings (3 sliders 10..255)
 *   - Debug tab enable (checkbox; takes effect next time Settings opens)
 *
 * All values persisted via g_appConfig setters (write /config.json on SD).
 */
class GeneralSettingsScreen {
public:
    using BrightnessCallback = std::function<void(uint8_t)>;

    /** Build the panel UI inside parent (a tab content container). */
    void create(lv_obj_t* parent);

    /** Live brightness preview while a slider is dragged. */
    void setOnMainBrightnessChange(BrightnessCallback cb)     { _onMainBri     = cb; }
    void setOnAlarmBrightnessChange(BrightnessCallback cb)    { _onAlarmBri    = cb; }
    void setOnSettingsBrightnessChange(BrightnessCallback cb) { _onSettingsBri = cb; }

    // Public so the anon-namespace helper buildSpinRow() can attach them as
    // event callbacks for the dec/inc step buttons.
    static void _spinDecCb(lv_event_t* e);
    static void _spinIncCb(lv_event_t* e);

private:
    lv_obj_t* _root           = nullptr;
    lv_obj_t* _spinSnooze     = nullptr;
    lv_obj_t* _spinMaxAlarm   = nullptr;
    lv_obj_t* _spinInactivity = nullptr;
    lv_obj_t* _sliderMain     = nullptr;
    lv_obj_t* _sliderAlarm    = nullptr;
    lv_obj_t* _sliderSettings = nullptr;
    lv_obj_t* _chkDebug       = nullptr;

    BrightnessCallback _onMainBri     = nullptr;
    BrightnessCallback _onAlarmBri    = nullptr;
    BrightnessCallback _onSettingsBri = nullptr;

    static void _snoozeValueCb(lv_event_t* e);
    static void _maxAlarmValueCb(lv_event_t* e);
    static void _inactivityValueCb(lv_event_t* e);
    static void _mainBriCb(lv_event_t* e);
    static void _alarmBriCb(lv_event_t* e);
    static void _settingsBriCb(lv_event_t* e);
    static void _debugChkCb(lv_event_t* e);
};

extern GeneralSettingsScreen generalSettingsScreen;
