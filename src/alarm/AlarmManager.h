#pragma once
#include <Arduino.h>
#include <time.h>
#include <vector>
#include <set>
#include <functional>

/**
 * Sound source selector for an alarm.
 *  - SoundType::Stream → use streamUrl (web radio, ICY/HTTP MP3)
 *  - SoundType::SD     → use soundPath (MP3 file on SD card, e.g. "/music/foo.mp3")
 */
enum class SoundType : uint8_t {
    Stream = 0,
    SD     = 1,
};

struct Alarm {
    uint32_t  id        = 0;            // stable id (assigned by AlarmManager)
    String    title;                    // user-editable, e.g. "Wecker Werktag"
    uint8_t   hour      = 7;
    uint8_t   minute    = 0;
    uint8_t   weekdays  = 0;            // bitmask: bit 0 = Sun … bit 6 = Sat. 0 = one-shot.
    bool      enabled   = true;
    SoundType soundType = SoundType::Stream;
    String    streamUrl;                // when soundType == Stream
    String    soundPath;                // when soundType == SD
    uint8_t   volume    = 12;           // 0..21

    /** True if no weekday bits are set → one-shot alarm. */
    bool isOneShot() const { return weekdays == 0; }

    /**
     * Matches "now" — same hour+minute and (one-shot OR matching weekday bit).
     * Ignores master switch and skip-set.
     */
    bool matchesNow(const tm& now) const {
        if (!enabled) return false;
        if (now.tm_hour != hour || now.tm_min != minute) return false;
        return isOneShot() || (weekdays & (1 << now.tm_wday));
    }
};

using AlarmCallback = std::function<void(const Alarm&)>;

/**
 * AlarmManager — loads/saves alarms from SD /alarms.json.
 */
class AlarmManager {
public:
    /** Load from SD; auto-seeds /alarms.json if missing. */
    void begin();

    /** Call from loop() once per second with the current local time. */
    void check(const tm& now);

    void setTriggerCallback(AlarmCallback cb) { _onTrigger = cb; }

    // ---- CRUD ----------------------------------------------------------
    void addAlarm(const Alarm& alarm);                  // assigns id, persists
    void updateAlarm(size_t index, const Alarm& alarm); // keeps id, persists
    void removeAlarm(size_t index);

    const std::vector<Alarm>& alarms() const { return _alarms; }
    Alarm*       at(size_t i)       { return (i < _alarms.size()) ? &_alarms[i] : nullptr; }
    const Alarm* at(size_t i) const { return (i < _alarms.size()) ? &_alarms[i] : nullptr; }

    // ---- Master switch -------------------------------------------------
    void setMasterEnabled(bool en);
    bool isMasterEnabled() const { return _masterEnabled; }

    // ---- Skip / Unskip (in-RAM only) ----------------------------------
    /** Skip the next upcoming alarm so it does not fire (one-time). */
    void skipNext();
    /** Undo the most recent skip (LIFO). */
    void unskip();

    /**
     * Compute the next alarm to fire (respecting master switch, enabled
     * flags and skip-set). Searches up to 8 days ahead.
     * @return false if no upcoming alarm.
     */
    bool nextAlarm(const tm& now, size_t* outIndex, tm* outFire) const;

    void save();
    void load();

private:
    std::vector<Alarm>    _alarms;
    std::set<uint32_t>    _skippedIds;
    std::vector<uint32_t> _skipOrder;
    AlarmCallback         _onTrigger;
    bool                  _masterEnabled = true;

    uint8_t _lastFiredMinute = 0xFF;
    int8_t  _lastFiredHour   = -1;
    int8_t  _lastFiredWday   = -1;

    uint32_t _nextId = 1;

    void _seedDefaults();
    uint32_t _allocId();
    /** Pure helper — returns true if alarm `a` next fires within 8 days of base. */
    static bool _computeNextFire(const Alarm& a, const tm& base, tm* out);
};
