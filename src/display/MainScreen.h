#pragma once
#include <Arduino.h>
#include <time.h>
#include <lvgl.h>

/**
 * MainScreen
 *
 * Builds and owns the primary LVGL UI: status bar, clock/date panel,
 * sensor strip, and weather panel. All weather and sensor areas are
 * placeholder-only until the respective subsystems are wired up.
 *
 * Usage:
 *   1. Call create() once after DisplayManager::begin().
 *   2. Call updateTime() and updateWifi() from loop() (e.g. once/second).
 */
class MainScreen {
public:
    /** Build all LVGL widgets on lv_scr_act(). */
    void create();

    /**
     * Refresh the date and time labels.
     * @param t  Current local time from TimeManager::now().
     */
    void updateTime(const struct tm& t);

    /**
     * Refresh the WiFi status bar.
     * @param ssid     Network name, e.g. "MyWiFi" or "Not Connected"
     * @param ip       IP address string, e.g. "192.168.1.42" or "---"
     * @param quality  Signal quality 0–100
     */
    void updateWifi(const char* ssid, const char* ip, int quality);

    /**
     * Refresh the sensor strip values + colors.
     * Color thresholds match the Radiowecker_EEZ_AI reference project.
     * @param temp  Temperature in °C
     * @param hum   Relative humidity in %RH
     * @param co2   eCO2 in ppm
     * @param tvoc  TVOC in ppb
     */
    void updateSensors(float temp, float hum, uint16_t co2, uint16_t tvoc);

private:
    // Status bar labels
    lv_obj_t* _lblWifiName    = nullptr;
    lv_obj_t* _lblIP          = nullptr;
    lv_obj_t* _lblWifiQuality = nullptr;

    // Clock panel labels
    lv_obj_t* _lblDate        = nullptr;
    lv_obj_t* _lblTime        = nullptr;
    lv_obj_t* _lblNextAlarm   = nullptr;

    // Sensor strip value labels (TEMP, HUM, CO2, TVOC)
    lv_obj_t* _lblTemp        = nullptr;
    lv_obj_t* _lblHum         = nullptr;
    lv_obj_t* _lblCO2         = nullptr;
    lv_obj_t* _lblTVOC        = nullptr;

    static const char* _germanDay(int wday);

    static lv_obj_t* _buildWeatherTile(lv_obj_t* parent, int yOfs, int height,
                                        const char* title, bool isCurrent);
};
