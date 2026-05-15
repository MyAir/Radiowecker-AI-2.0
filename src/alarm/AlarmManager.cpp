#include "AlarmManager.h"
#include <LittleFS.h>
#include <ArduinoJson.h>
#include "../config.h"

void AlarmManager::begin() {
    load();
}

void AlarmManager::check(const tm& now) {
    // Fire at most once per minute
    if (now.tm_min == _lastMinute) return;
    if (now.tm_sec != 0) return;          // fire on the exact minute boundary
    _lastMinute = now.tm_min;

    for (const Alarm& alarm : _alarms) {
        if (alarm.matchesNow(now)) {
            Serial.printf("[Alarm] Triggered at %02d:%02d\n", alarm.hour, alarm.minute);
            if (_onTrigger) _onTrigger(alarm);
        }
    }
}

void AlarmManager::addAlarm(const Alarm& alarm) {
    _alarms.push_back(alarm);
    save();
}

void AlarmManager::removeAlarm(size_t index) {
    if (index < _alarms.size()) {
        _alarms.erase(_alarms.begin() + index);
        save();
    }
}

void AlarmManager::save() {
    JsonDocument doc;
    JsonArray arr = doc["alarms"].to<JsonArray>();
    for (const Alarm& a : _alarms) {
        JsonObject obj = arr.add<JsonObject>();
        obj["hour"]      = a.hour;
        obj["minute"]    = a.minute;
        obj["weekdays"]  = a.weekdays;
        obj["enabled"]   = a.enabled;
        obj["streamUrl"] = a.streamUrl;
    }

    File f = LittleFS.open(CONFIG_FILE, "r+");
    if (!f) f = LittleFS.open(CONFIG_FILE, "w");
    if (!f) { Serial.println("[Alarm] Cannot open config for writing"); return; }

    // Merge into existing config JSON
    JsonDocument existing;
    deserializeJson(existing, f);
    f.seek(0);
    existing["alarms"] = doc["alarms"];
    serializeJson(existing, f);
    f.close();
}

void AlarmManager::load() {
    if (!LittleFS.exists(CONFIG_FILE)) return;

    File f = LittleFS.open(CONFIG_FILE, "r");
    if (!f) return;

    JsonDocument doc;
    if (deserializeJson(doc, f) != DeserializationError::Ok) { f.close(); return; }
    f.close();

    _alarms.clear();
    for (JsonObject obj : doc["alarms"].as<JsonArray>()) {
        Alarm a;
        a.hour      = obj["hour"]      | 7;
        a.minute    = obj["minute"]    | 0;
        a.weekdays  = obj["weekdays"]  | 0;
        a.enabled   = obj["enabled"]   | true;
        a.streamUrl = obj["streamUrl"] | "";
        _alarms.push_back(a);
    }
    Serial.printf("[Alarm] Loaded %u alarm(s)\n", _alarms.size());
}
