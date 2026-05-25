#include "AlarmManager.h"
#include <SD.h>
#include <ArduinoJson.h>
#include "../config.h"
#include "../serial_safe.h"

static constexpr const char* ALARMS_FILE = "/alarms.json";
static constexpr int SCHEMA_VERSION = 1;

// ---------------------------------------------------------------------------
// begin() — load from SD, seed defaults if missing
// ---------------------------------------------------------------------------
void AlarmManager::begin() {
    if (!SD.exists(ALARMS_FILE)) {
        serial_safe_println("[Alarm] /alarms.json missing — seeding defaults");
        _seedDefaults();
        save();
        return;
    }
    load();
}

// ---------------------------------------------------------------------------
// _seedDefaults() — two example alarms
// ---------------------------------------------------------------------------
void AlarmManager::_seedDefaults() {
    _alarms.clear();
    _nextId = 1;

    Alarm a1;
    a1.id        = _allocId();
    a1.title     = "Wecker Werktag";
    a1.hour      = 6;
    a1.minute    = 30;
    a1.weekdays  = (1<<1)|(1<<2)|(1<<3)|(1<<4)|(1<<5);  // Mon..Fri
    a1.enabled   = true;
    a1.soundType = SoundType::Stream;
    a1.streamUrl = "http://stream.srg-ssr.ch/m/drs3/mp3_128";
    a1.volume    = 12;
    _alarms.push_back(a1);

    Alarm a2;
    a2.id        = _allocId();
    a2.title     = "Wochenende";
    a2.hour      = 8;
    a2.minute    = 0;
    a2.weekdays  = (1<<0)|(1<<6);                       // Sun, Sat
    a2.enabled   = true;
    a2.soundType = SoundType::SD;
    a2.soundPath = "/Chef316.mp3";
    a2.volume    = 14;
    _alarms.push_back(a2);
}

// ---------------------------------------------------------------------------
// check() — fire matching alarms at minute boundary
// ---------------------------------------------------------------------------
void AlarmManager::check(const tm& now) {
    if (!_masterEnabled) return;
    // Fire exactly once per minute (when seconds == 0 and we haven't fired this
    // hour:minute yet)
    if (now.tm_sec != 0) return;
    if (now.tm_hour == _lastFiredHour &&
        now.tm_min  == _lastFiredMinute &&
        now.tm_wday == _lastFiredWday) return;

    _lastFiredHour   = now.tm_hour;
    _lastFiredMinute = now.tm_min;
    _lastFiredWday   = now.tm_wday;

    for (Alarm& alarm : _alarms) {
        if (!alarm.matchesNow(now)) continue;

        // Skip set?
        auto it = _skippedIds.find(alarm.id);
        if (it != _skippedIds.end()) {
            _skippedIds.erase(it);
            // also remove from _skipOrder
            for (auto so = _skipOrder.begin(); so != _skipOrder.end(); ++so) {
                if (*so == alarm.id) { _skipOrder.erase(so); break; }
            }
            serial_safe_printf("[Alarm] skipped id=%u (%02d:%02d)\n",
                               alarm.id, alarm.hour, alarm.minute);
            continue;
        }

        serial_safe_printf("[Alarm] triggered id=%u %02d:%02d '%s'\n",
                           alarm.id, alarm.hour, alarm.minute,
                           alarm.title.c_str());

        // One-shot auto-disables itself after firing
        if (alarm.isOneShot()) {
            alarm.enabled = false;
            save();
        }
        if (_onTrigger) _onTrigger(alarm);
    }
}

// ---------------------------------------------------------------------------
// CRUD
// ---------------------------------------------------------------------------
uint32_t AlarmManager::_allocId() {
    return _nextId++;
}

void AlarmManager::addAlarm(const Alarm& alarm) {
    Alarm a = alarm;
    if (a.id == 0) a.id = _allocId();
    if (a.id >= _nextId) _nextId = a.id + 1;
    _alarms.push_back(a);
    save();
}

void AlarmManager::updateAlarm(size_t index, const Alarm& alarm) {
    if (index >= _alarms.size()) return;
    Alarm a = alarm;
    a.id = _alarms[index].id;  // preserve id
    _alarms[index] = a;
    save();
}

void AlarmManager::removeAlarm(size_t index) {
    if (index >= _alarms.size()) return;
    const uint32_t id = _alarms[index].id;
    _alarms.erase(_alarms.begin() + index);
    // clean up skip-set
    _skippedIds.erase(id);
    for (auto it = _skipOrder.begin(); it != _skipOrder.end(); ) {
        if (*it == id) it = _skipOrder.erase(it); else ++it;
    }
    save();
}

void AlarmManager::setMasterEnabled(bool en) {
    _masterEnabled = en;
    save();
}

// ---------------------------------------------------------------------------
// Skip / Unskip
// ---------------------------------------------------------------------------
void AlarmManager::skipNext() {
    // Need a current time to compute "next" — we recompute against the
    // most recent loaded tm via nextAlarm(). The caller is expected to
    // pass us "now" through that API; here we just need the id, so we
    // do the work in the public skip wrapper. To keep AlarmManager
    // self-contained without an extra time arg, we use the libc time().
    time_t t = time(nullptr);
    tm now;
    localtime_r(&t, &now);

    size_t idx = 0;
    tm fire{};
    if (!nextAlarm(now, &idx, &fire)) {
        serial_safe_println("[Alarm] skipNext: no upcoming alarm");
        return;
    }
    const uint32_t id = _alarms[idx].id;
    if (_skippedIds.insert(id).second) {
        _skipOrder.push_back(id);
        serial_safe_printf("[Alarm] skip id=%u (%02d:%02d '%s')\n",
                           id, _alarms[idx].hour, _alarms[idx].minute,
                           _alarms[idx].title.c_str());
    }
}

void AlarmManager::unskip() {
    if (_skipOrder.empty()) {
        serial_safe_println("[Alarm] unskip: nothing skipped");
        return;
    }
    const uint32_t id = _skipOrder.back();
    _skipOrder.pop_back();
    _skippedIds.erase(id);
    serial_safe_printf("[Alarm] unskip id=%u\n", id);
}

// ---------------------------------------------------------------------------
// _computeNextFire — minutes-until until the next fire for `a` after `base`,
// returns the tm of that fire in *out. Returns false if no fire within 8 days
// (covers all weekday combinations + one-shot alarms).
// ---------------------------------------------------------------------------
bool AlarmManager::_computeNextFire(const Alarm& a, const tm& base, tm* out) {
    if (!a.enabled) return false;

    // Build a struct tm for "today at a.hour:a.minute:0", then advance day
    // by day until weekday matches (or one-shot fires once).
    tm cand = base;
    cand.tm_hour = a.hour;
    cand.tm_min  = a.minute;
    cand.tm_sec  = 0;

    // If candidate has already passed today, start at tomorrow.
    bool sameMinute = (base.tm_hour == a.hour && base.tm_min == a.minute);
    bool past       = (base.tm_hour > a.hour) ||
                      (base.tm_hour == a.hour && base.tm_min > a.minute);

    if (a.isOneShot()) {
        // One-shot fires today if not yet past, else tomorrow.
        if (past) {
            time_t t = mktime(&cand);
            t += 24 * 3600;
            localtime_r(&t, &cand);
        }
        *out = cand;
        return true;
    }

    // Repeating: search up to 8 days starting today.
    if (past || (sameMinute && base.tm_sec > 0)) {
        time_t t = mktime(&cand);
        t += 24 * 3600;
        localtime_r(&t, &cand);
    }
    for (int i = 0; i < 8; ++i) {
        if (a.weekdays & (1 << cand.tm_wday)) {
            *out = cand;
            return true;
        }
        time_t t = mktime(&cand);
        t += 24 * 3600;
        localtime_r(&t, &cand);
    }
    return false;
}

bool AlarmManager::nextAlarm(const tm& now, size_t* outIndex, tm* outFire) const {
    if (!_masterEnabled) return false;
    if (_alarms.empty()) return false;

    time_t bestT = 0;
    size_t bestIdx = 0;
    bool   found = false;

    for (size_t i = 0; i < _alarms.size(); ++i) {
        const Alarm& a = _alarms[i];
        if (!a.enabled) continue;

        tm fire{};
        if (!_computeNextFire(a, now, &fire)) continue;

        // If this alarm is in the skip-set, advance to its NEXT-after-next.
        if (_skippedIds.count(a.id) > 0) {
            // Move base past `fire` by 1 minute and recompute.
            tm after = fire;
            time_t t = mktime(&after);
            t += 60;
            localtime_r(&t, &after);
            tm fire2{};
            if (!_computeNextFire(a, after, &fire2)) continue;
            fire = fire2;
        }

        time_t ft = mktime(&fire);
        if (!found || ft < bestT) {
            found   = true;
            bestT   = ft;
            bestIdx = i;
            *outFire = fire;
        }
    }
    if (found) *outIndex = bestIdx;
    return found;
}

// ---------------------------------------------------------------------------
// save() / load() — SD /alarms.json
// ---------------------------------------------------------------------------
void AlarmManager::save() {
    JsonDocument doc;
    doc["schemaVersion"] = SCHEMA_VERSION;
    doc["masterEnabled"] = _masterEnabled;
    JsonArray arr = doc["alarms"].to<JsonArray>();
    for (const Alarm& a : _alarms) {
        JsonObject obj = arr.add<JsonObject>();
        obj["id"]        = a.id;
        obj["title"]     = a.title;
        obj["hour"]      = a.hour;
        obj["minute"]    = a.minute;
        obj["weekdays"]  = a.weekdays;
        obj["enabled"]   = a.enabled;
        obj["soundType"] = (a.soundType == SoundType::SD) ? "sd" : "stream";
        obj["streamUrl"] = a.streamUrl;
        obj["soundPath"] = a.soundPath;
        obj["volume"]    = a.volume;
    }

    File f = SD.open(ALARMS_FILE, FILE_WRITE);
    if (!f) {
        serial_safe_println("[Alarm] cannot open /alarms.json for writing");
        return;
    }
    if (serializeJson(doc, f) == 0) {
        serial_safe_println("[Alarm] serializeJson wrote 0 bytes");
    }
    f.close();
}

void AlarmManager::load() {
    File f = SD.open(ALARMS_FILE, FILE_READ);
    if (!f) {
        serial_safe_println("[Alarm] cannot open /alarms.json for reading");
        _seedDefaults();
        return;
    }
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, f);
    f.close();
    if (err) {
        serial_safe_printf("[Alarm] parse error %s — re-seeding\n", err.c_str());
        _seedDefaults();
        save();
        return;
    }

    _masterEnabled = doc["masterEnabled"] | true;
    _alarms.clear();
    _nextId = 1;
    for (JsonObject obj : doc["alarms"].as<JsonArray>()) {
        Alarm a;
        a.id        = obj["id"]        | 0u;
        a.title     = (const char*)(obj["title"] | "");
        a.hour      = obj["hour"]      | 7;
        a.minute    = obj["minute"]    | 0;
        a.weekdays  = obj["weekdays"]  | 0;
        a.enabled   = obj["enabled"]   | true;
        const char* st = obj["soundType"] | "stream";
        a.soundType = (strcmp(st, "sd") == 0) ? SoundType::SD : SoundType::Stream;
        a.streamUrl = (const char*)(obj["streamUrl"] | "");
        a.soundPath = (const char*)(obj["soundPath"] | "");
        a.volume    = obj["volume"]    | 12;
        if (a.id >= _nextId) _nextId = a.id + 1;
        if (a.id == 0)       a.id = _allocId();
        _alarms.push_back(a);
    }
    serial_safe_printf("[Alarm] loaded %u alarm(s), masterEnabled=%d\n",
                       (unsigned)_alarms.size(), (int)_masterEnabled);
}
