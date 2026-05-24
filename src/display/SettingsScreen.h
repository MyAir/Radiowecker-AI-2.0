#pragma once
#include <Arduino.h>
#include <lvgl.h>

/**
 * SettingsScreen
 *
 * Slides in over the main screen from the left.  Contains:
 *   - Working back button (top-left)  → returns to main screen
 *   - Navigation buttons: "System", "Alarms"
 *   - 30 s inactivity timeout → auto-returns to main screen
 *
 * Call create() from within an LVGL event callback (no lv_lock() needed).
 * Wire callbacks before calling create().
 */
class SettingsScreen {
public:
    using Callback = void(*)();

    /** Build the settings LVGL screen and slide it in over mainScr. */
    void create(lv_obj_t* mainScr);

    /** Tap on "Alarms" button → caller opens AlarmSetupScreen.
     *  The settings screen has already been torn down when this fires. */
    void setOnOpenAlarms(Callback cb)                 { _onOpenAlarms = cb; }
    /** Tap on "System" button. Same lifetime contract. */
    void setOnOpenGeneral(Callback cb)                { _onOpenGeneral = cb; }

private:
    lv_obj_t*          _scr              = nullptr;
    lv_obj_t*          _mainScr          = nullptr;
    lv_timer_t*        _timer            = nullptr;

    Callback           _onOpenAlarms  = nullptr;
    Callback           _onOpenGeneral = nullptr;

    /** Load main screen, delete this screen after animation. */
    void _goBack();

    // Timer callback
    static void _timeoutCb(lv_timer_t* t);

    // Button callbacks
    static void _backBtnCb(lv_event_t* e);
    static void _openAlarmsCb(lv_event_t* e);
    static void _openGeneralCb(lv_event_t* e);
    /** Tear down this settings screen so AlarmSetup/GeneralSettings can
     *  take over without animation collision. The next loaded screen owns
     *  the deletion via lv_screen_load_anim(..., auto_del=true). */
    void _detachForChildScreen();

    /** Create a styled button. Returns the button object. */
    lv_obj_t* _makeBtn(lv_obj_t* parent, const char* label,
                        int x, int y, int w, int h,
                        uint32_t textColor, bool dimBorder = false);
};
