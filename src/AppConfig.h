#pragma once
#include <Arduino.h>

/**
 * AppConfig — small persisted app-wide settings read from / written to
 * SD /config.json. The file may also contain a project marker
 * ("project": "radiowecker2"); AppConfig preserves unknown fields.
 *
 * Currently stored fields:
 *   snoozeMinutes — alarm snooze interval in minutes (1..60, default 9)
 */
class AppConfig {
public:
    /** Load values from SD /config.json. Missing/invalid → defaults. */
    void load();
    /** Persist to SD /config.json (merges with existing JSON). */
    void save();

    uint16_t snoozeMinutes() const { return _snoozeMinutes; }
    void     setSnoozeMinutes(uint16_t m);

private:
    uint16_t _snoozeMinutes = 9;
};

extern AppConfig g_appConfig;
