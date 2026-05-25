#pragma once
#include <Arduino.h>
#include <lvgl.h>

/**
 * SettingsScreen
 *
 * Slides in over the main screen from the left. Hosts an lv_tabview with
 * three tabs: "Alarm" (AlarmSetupScreen), "System" (GeneralSettingsScreen)
 * and optionally "Debug" (DebugScreen, visible only when
 * g_appConfig.debugEnabled() is true).
 *
 * Inactivity timeout is configurable via g_appConfig.inactivityTimeoutSeconds()
 * (0 = disabled). When elapsed the screen slides back to mainScr.
 *
 * The child screens build their UI into the tab content containers exposed
 * here. SettingsScreen owns the tabview lifecycle and the back button which
 * lives in the tab strip next to the tab labels.
 */
class SettingsScreen {
public:
    /** Build the settings screen, create the tabview, and instantiate the
     *  child panels. Wire all child callbacks BEFORE calling this. */
    void create(lv_obj_t* mainScr);

    /** Returns true while the tabview screen is the active LVGL screen. */
    bool isVisible() const { return _scr != nullptr; }

    /** Tear down (e.g. called from main when WiFi portal becomes active). */
    void close() { _goBack(); }

private:
    lv_obj_t*   _scr       = nullptr;
    lv_obj_t*   _mainScr   = nullptr;
    lv_obj_t*   _tabview   = nullptr;
    lv_obj_t*   _tabAlarm  = nullptr;
    lv_obj_t*   _tabSystem = nullptr;
    lv_obj_t*   _tabWeather= nullptr;
    lv_obj_t*   _tabDebug  = nullptr;   // nullptr when hidden
    lv_obj_t*   _backBtn   = nullptr;
    lv_timer_t* _timer     = nullptr;

    void _goBack();

    static void _backBtnCb(lv_event_t* e);
    static void _timeoutCb(lv_timer_t* t);
};

extern SettingsScreen settingsScreen;
