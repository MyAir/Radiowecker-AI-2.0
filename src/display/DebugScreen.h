#pragma once
#include <Arduino.h>
#include <functional>
#include <lvgl.h>

/**
 * DebugScreen — "Debug" tab inside SettingsScreen.
 *
 * Currently hosts the alarm-test UI that used to live on the System tab.
 * Only shown when g_appConfig.debugEnabled() is true at the moment the
 * settings screen is opened.
 */
class DebugScreen {
public:
    using TestAlarmCallback = std::function<void(size_t)>;

    /** Build the panel UI inside parent (a tab content container). */
    void create(lv_obj_t* parent);

    /** Set newline-separated dropdown options ("HH:MM Title\n…"). */
    void setAlarmOptions(const char* options);

    /** Null all widget pointers — call after the host screen is destroyed
     *  to prevent dangling-pointer use on the next settings-open. */
    void invalidate() { _root = nullptr; _alarmDrop = nullptr; _testBtn = nullptr; }

    void setOnTestAlarm(TestAlarmCallback cb) { _onTestAlarm = cb; }

private:
    lv_obj_t* _root         = nullptr;
    lv_obj_t* _alarmDrop    = nullptr;
    lv_obj_t* _testBtn      = nullptr;
    TestAlarmCallback _onTestAlarm = nullptr;

    static void _testBtnCb(lv_event_t* e);
};

extern DebugScreen debugScreen;
