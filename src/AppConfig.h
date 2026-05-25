#pragma once
#include <Arduino.h>

/**
 * AppConfig — small persisted app-wide settings read from / written to
 * SD /config.json. The file may also contain a project marker
 * ("project": "radiowecker2"); AppConfig preserves unknown fields.
 *
 * Stored fields:
 *   snoozeMinutes             — alarm snooze interval in minutes (1..60, default 9)
 *   mainBrightness            — display brightness on MainScreen        (10..255, default 128)
 *   alarmBrightness           — display brightness on AlarmScreen       (10..255, default 255)
 *   settingsBrightness        — display brightness on SettingsScreen    (10..255, default 180)
 *   maxAlarmDurationMinutes   — auto-stop alarm after N min (0 = off)   (0..60,  default 5)
 *   inactivityTimeoutSeconds  — settings inactivity timeout (0 = off)   (0..300, default 30)
 *   debugEnabled              — show the Debug tab in Settings          (default false)
 *   deviceName                — mDNS / OTA / WiFi hostname (1..30 chars, default "radiowecker2")
 */
class AppConfig {
public:
    /** Load values from SD /config.json. Missing/invalid → defaults. */
    void load();
    /** Persist to SD /config.json (merges with existing JSON). */
    void save();

    uint16_t snoozeMinutes() const            { return _snoozeMinutes; }
    void     setSnoozeMinutes(uint16_t m);

    uint8_t  mainBrightness() const           { return _mainBrightness; }
    void     setMainBrightness(uint8_t v);

    uint8_t  alarmBrightness() const          { return _alarmBrightness; }
    void     setAlarmBrightness(uint8_t v);

    uint8_t  settingsBrightness() const       { return _settingsBrightness; }
    void     setSettingsBrightness(uint8_t v);

    uint16_t maxAlarmDurationMinutes() const  { return _maxAlarmDurationMinutes; }
    void     setMaxAlarmDurationMinutes(uint16_t m);

    uint16_t inactivityTimeoutSeconds() const { return _inactivityTimeoutSeconds; }
    void     setInactivityTimeoutSeconds(uint16_t s);

    bool     debugEnabled() const             { return _debugEnabled; }
    void     setDebugEnabled(bool v);

    const String& deviceName() const          { return _deviceName; }
    void     setDeviceName(const String& name);

private:
    uint16_t _snoozeMinutes            = 9;
    uint8_t  _mainBrightness           = 128;
    uint8_t  _alarmBrightness          = 255;
    uint8_t  _settingsBrightness       = 180;
    uint16_t _maxAlarmDurationMinutes  = 5;
    uint16_t _inactivityTimeoutSeconds = 30;
    bool     _debugEnabled             = false;
    String   _deviceName               = "radiowecker2";
};

extern AppConfig g_appConfig;
