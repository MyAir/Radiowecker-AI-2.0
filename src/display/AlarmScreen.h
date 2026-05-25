#pragma once
#include <Arduino.h>
#include <functional>
#include <lvgl.h>
#include "alarm/AlarmManager.h"
#include "weather/WeatherManager.h"

/**
 * AlarmScreen — full-screen UI shown while an alarm is firing.
 *
 * Layout (alarm_screen_3 = hero current weather + small forecast row):
 *   - Header bar:   alarm title (left), live date+time (right)
 *   - Hero "Jetzt": weather temperature + description card (left side)
 *   - Big clock:    HH:MM digital (right side)
 *   - Now-playing:  ICY/ID3 metadata strip (below clock)
 *   - Forecast row: 3 tiles (Morgen früh / Nachmittag / Morgen)
 *   - Buttons:      [Schlummern] + [Stop]
 *
 * Lifecycle:
 *   show(a, brightness) — saves brightness, sets panel to 255, slides in.
 *   hide()              — restores brightness, slides back to main, frees screen.
 *   update(now, weather, audio) — call once per second from loop().
 *
 * Snooze starts an lv_timer that re-fires the alarm action after
 * g_appConfig.snoozeMinutes() * 60 s.
 */
class AlarmScreen {
public:
    using SimpleCallback     = std::function<void()>;
    using BrightnessCallback = std::function<void(uint8_t)>;
    using TriggerCallback    = std::function<void(const Alarm&)>;

    /** Build screen and slide in from below. mainScr = the MainScreen to
     *  return to. brightness = current saved value (will be restored on hide). */
    void show(lv_obj_t* mainScr, const Alarm& a, uint8_t currentBrightness);

    /** Slide back to mainScr; AlarmScreen is auto-deleted by LVGL. */
    void hide();

    /** True between show() and hide(). */
    bool isVisible() const { return _scr != nullptr; }

    /** Update live time, weather and now-playing metadata. Cheap to call
     *  every second; only touches widgets whose source data changed. */
    void tick(const tm& now, const class WeatherManager& w,
              const class AudioPlayer& audio);

    // Callbacks the host (main.cpp) must wire on startup:
    void setOnBrightness(BrightnessCallback cb)   { _onBrightness = cb; }
    void setOnSnoozeFire(TriggerCallback cb)      { _onSnoozeFire = cb; }
    void setOnStop(SimpleCallback cb)             { _onStop       = cb; }

private:
    lv_obj_t* _scr      = nullptr;
    lv_obj_t* _mainScr  = nullptr;

    // Header
    lv_obj_t* _lblTitle    = nullptr;
    lv_obj_t* _lblDate     = nullptr;
    lv_obj_t* _lblTimeHdr  = nullptr;

    // Hero
    lv_obj_t* _lblHeroTemp = nullptr;
    lv_obj_t* _lblHeroUnit = nullptr;
    lv_obj_t* _lblHeroDesc = nullptr;
    lv_obj_t* _imgHeroIcon = nullptr;
    char      _heroIconCode[8] = {0};

    // Big clock + now-playing
    lv_obj_t* _lblBigClock = nullptr;
    lv_obj_t* _lblMeta     = nullptr;

    // Forecast tiles (label triplets: head/temp/pop)
    struct Tile {
        lv_obj_t* head=nullptr; lv_obj_t* temp=nullptr; lv_obj_t* pop=nullptr;
        lv_obj_t* icon=nullptr;
        char      iconCode[8] = {0};
    };
    Tile _tMorn;
    Tile _tAft;
    Tile _tEve;
    Tile _tTom;

    // Buttons
    lv_obj_t* _btnSnooze = nullptr;
    lv_obj_t* _btnStop   = nullptr;

    // Snooze re-fire timer
    lv_timer_t* _snoozeTimer = nullptr;
    // 0 = not snoozing; otherwise lv_tick value at which the alarm re-fires.
    uint32_t  _snoozeTargetTick = 0;
    int       _snoozeLastSec   = -1;

    // State
    Alarm     _alarm;
    uint8_t   _savedBrightness = 200;
    uint32_t  _lastMetaVersion = 0;
    int       _lastMinute      = -1;
    int       _lastWeatherStamp= -1;

    // Callbacks
    BrightnessCallback _onBrightness = nullptr;
    TriggerCallback    _onSnoozeFire = nullptr;
    SimpleCallback     _onStop       = nullptr;

    // LVGL event cbs
    static void _snoozeBtnCb(lv_event_t* e);
    static void _stopBtnCb(lv_event_t* e);
    static void _snoozeTimerCb(lv_timer_t* t);
};

extern AlarmScreen alarmScreen;
