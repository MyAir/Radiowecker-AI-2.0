#pragma once
#include <Arduino.h>
#include <time.h>
#include <vector>
#include <functional>

struct Alarm {
    uint8_t  hour;
    uint8_t  minute;
    uint8_t  weekdays;   // bitmask: bit 0 = Sunday … bit 6 = Saturday
    bool     enabled;
    String   streamUrl;  // empty = use SD alarm sound

    bool matchesNow(const tm& now) const {
        if (!enabled) return false;
        if (now.tm_hour != hour || now.tm_min != minute) return false;
        return (weekdays == 0) || (weekdays & (1 << now.tm_wday));
    }
};

using AlarmCallback = std::function<void(const Alarm&)>;

class AlarmManager {
public:
    void begin();                        // loads alarms from LittleFS
    void check(const tm& now);          // call from loop() every second

    void setTriggerCallback(AlarmCallback cb) { _onTrigger = cb; }

    void addAlarm(const Alarm& alarm);
    void removeAlarm(size_t index);
    const std::vector<Alarm>& alarms() const { return _alarms; }

    /** Master on/off switch — persisted to LittleFS. */
    void setMasterEnabled(bool en) { _masterEnabled = en; save(); }
    bool isMasterEnabled() const   { return _masterEnabled; }

    void save();                         // persists to LittleFS
    void load();

private:
    std::vector<Alarm> _alarms;
    AlarmCallback      _onTrigger;
    bool               _masterEnabled = true;

    uint8_t _lastMinute = 0xFF;          // prevents double-firing within the same minute
};
