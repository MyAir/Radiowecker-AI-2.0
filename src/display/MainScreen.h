#pragma once
#include <Arduino.h>
#include <time.h>
#include <lvgl.h>

/**
 * MainScreen
 *
 * Full-width bedroom clock UI: status bar, weekday + date, large time,
 * next-alarm row with Skip button, and sensor strip at the bottom.
 *
 * Usage:
 *   1. Call create() once after DisplayManager::begin().
 *   2. Call updateTime() and updateWifi() from loop() (e.g. once/second).
 *   3. Call setNextAlarm() whenever the next alarm changes.
 *   4. Wire setOnSkipAlarm() to your alarm-manager's skip handler.
 */
class MainScreen {
public:
    /** Build all LVGL widgets on lv_scr_act(). */
    void create();

    /**
     * Refresh the weekday name, date and time labels.
     * @param t  Current local time from TimeManager::now().
     */
    void updateTime(const struct tm& t);

    /**
     * Refresh the WiFi status bar.
     * @param ssid     Network name or "Not Connected"
     * @param ip       IP address string or "---"
     * @param quality  Signal quality 0–100
     */
    void updateWifi(const char* ssid, const char* ip, int quality);

    /**
     * Refresh the sensor strip values + colors.
     * Color thresholds match the Radiowecker_EEZ_AI reference project.
     */
    void updateSensors(float temp, float hum, uint16_t co2, uint16_t tvoc);

    /**
     * Update the next-alarm display text (right half of alarm row).
     * @param text  e.g. "Mo 02.01.2026  06:30", or "" / nullptr → "---"
     */
    void setNextAlarm(const char* text);

    // Skip-alarm callback
    using SkipCallback = void(*)();
    void setOnSkipAlarm(SkipCallback cb) { _onSkipAlarm = cb; }

private:
    // Status bar
    lv_obj_t* _lblWifiName    = nullptr;
    lv_obj_t* _lblIP          = nullptr;
    lv_obj_t* _lblWifiQuality = nullptr;

    // Clock area
    lv_obj_t* _lblWeekday   = nullptr;
    lv_obj_t* _lblDate      = nullptr;
    lv_obj_t* _lblTime      = nullptr;
    lv_obj_t* _lblNextAlarm = nullptr;  // alarm value (right of caption)
    lv_obj_t* _btnSkipAlarm = nullptr;

    // Sensor strip value labels (TEMP, HUM, CO2, TVOC)
    lv_obj_t* _lblTemp  = nullptr;
    lv_obj_t* _lblHum   = nullptr;
    lv_obj_t* _lblCO2   = nullptr;
    lv_obj_t* _lblTVOC  = nullptr;

    SkipCallback _onSkipAlarm = nullptr;

    static void        _skipBtnEventCb(lv_event_t* e);
    static const char* _germanDay(int wday);
    static const char* _germanMonthShort(int mon);
};

