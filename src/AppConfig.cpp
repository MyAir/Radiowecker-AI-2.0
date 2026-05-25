#include "AppConfig.h"
#include <SD.h>
#include <ArduinoJson.h>
#include "serial_safe.h"

static constexpr const char* APP_CONFIG_FILE = "/config.json";

AppConfig g_appConfig;

void AppConfig::load() {
    if (!SD.exists(APP_CONFIG_FILE)) {
        serial_safe_println("[AppConfig] /config.json missing on SD, using defaults");
        return;
    }
    File f = SD.open(APP_CONFIG_FILE, FILE_READ);
    if (!f) return;
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, f);
    f.close();
    if (err) {
        serial_safe_printf("[AppConfig] parse error: %s\n", err.c_str());
        return;
    }
    if (!doc["snoozeMinutes"].isNull()) {
        uint16_t m = doc["snoozeMinutes"] | 9;
        if (m >= 1 && m <= 60) _snoozeMinutes = m;
    }
    if (!doc["mainBrightness"].isNull()) {
        int v = doc["mainBrightness"] | 128;
        if (v >= 10 && v <= 255) _mainBrightness = (uint8_t)v;
    }
    if (!doc["alarmBrightness"].isNull()) {
        int v = doc["alarmBrightness"] | 255;
        if (v >= 10 && v <= 255) _alarmBrightness = (uint8_t)v;
    }
    if (!doc["settingsBrightness"].isNull()) {
        int v = doc["settingsBrightness"] | 180;
        if (v >= 10 && v <= 255) _settingsBrightness = (uint8_t)v;
    }
    if (!doc["maxAlarmDurationMinutes"].isNull()) {
        int v = doc["maxAlarmDurationMinutes"] | 5;
        if (v >= 0 && v <= 60) _maxAlarmDurationMinutes = (uint16_t)v;
    }
    if (!doc["inactivityTimeoutSeconds"].isNull()) {
        int v = doc["inactivityTimeoutSeconds"] | 30;
        if (v >= 0 && v <= 300) _inactivityTimeoutSeconds = (uint16_t)v;
    }
    if (!doc["debugEnabled"].isNull()) {
        _debugEnabled = doc["debugEnabled"] | false;
    }
    if (!doc["deviceName"].isNull()) {
        const char* s = doc["deviceName"];
        if (s && *s) {
            String n(s);
            n.trim();
            if (n.length() > 0 && n.length() <= 30) _deviceName = n;
        }
    }
    serial_safe_printf("[AppConfig] snooze=%u brMain=%u brAlarm=%u brSet=%u "
                       "maxDur=%u inact=%u debug=%d\n",
                       _snoozeMinutes, _mainBrightness, _alarmBrightness,
                       _settingsBrightness, _maxAlarmDurationMinutes,
                       _inactivityTimeoutSeconds, (int)_debugEnabled);
}

void AppConfig::save() {
    // Read existing JSON first so we don't drop other keys (e.g. project marker)
    JsonDocument doc;
    if (SD.exists(APP_CONFIG_FILE)) {
        File f = SD.open(APP_CONFIG_FILE, FILE_READ);
        if (f) {
            deserializeJson(doc, f);
            f.close();
        }
    }
    if (doc["project"].isNull()) doc["project"] = "radiowecker2";
    doc["snoozeMinutes"]            = _snoozeMinutes;
    doc["mainBrightness"]           = _mainBrightness;
    doc["alarmBrightness"]          = _alarmBrightness;
    doc["settingsBrightness"]       = _settingsBrightness;
    doc["maxAlarmDurationMinutes"]  = _maxAlarmDurationMinutes;
    doc["inactivityTimeoutSeconds"] = _inactivityTimeoutSeconds;
    doc["debugEnabled"]             = _debugEnabled;
    doc["deviceName"]               = _deviceName;

    File f = SD.open(APP_CONFIG_FILE, FILE_WRITE);
    if (!f) {
        serial_safe_println("[AppConfig] cannot open /config.json for writing");
        return;
    }
    serializeJson(doc, f);
    f.close();
}

static inline uint8_t clampBrightness(uint8_t v) {
    if (v < 10) v = 10;
    return v;
}

void AppConfig::setSnoozeMinutes(uint16_t m) {
    if (m < 1)  m = 1;
    if (m > 60) m = 60;
    _snoozeMinutes = m;
    save();
}

void AppConfig::setMainBrightness(uint8_t v) {
    _mainBrightness = clampBrightness(v);
    save();
}

void AppConfig::setAlarmBrightness(uint8_t v) {
    _alarmBrightness = clampBrightness(v);
    save();
}

void AppConfig::setSettingsBrightness(uint8_t v) {
    _settingsBrightness = clampBrightness(v);
    save();
}

void AppConfig::setMaxAlarmDurationMinutes(uint16_t m) {
    if (m > 60) m = 60;
    _maxAlarmDurationMinutes = m;
    save();
}

void AppConfig::setInactivityTimeoutSeconds(uint16_t s) {
    if (s > 300) s = 300;
    _inactivityTimeoutSeconds = s;
    save();
}

void AppConfig::setDebugEnabled(bool v) {
    _debugEnabled = v;
    save();
}

void AppConfig::setDeviceName(const String& name) {
    // Sanitize to a DNS-safe hostname: lowercase, [a-z0-9-], no leading/
    // trailing hyphen, 1..30 chars. Anything else is dropped so the user
    // can't lock us out of mDNS/OTA.
    String n;
    n.reserve(name.length());
    for (size_t i = 0; i < name.length(); ++i) {
        char c = name[i];
        if (c >= 'A' && c <= 'Z') c = (char)(c + 32);
        if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-') {
            n += c;
        }
    }
    while (n.startsWith("-")) n.remove(0, 1);
    while (n.endsWith("-"))   n.remove(n.length() - 1);
    if (n.length() == 0) return;
    if (n.length() > 30) n = n.substring(0, 30);
    if (n == _deviceName) return;
    _deviceName = n;
    save();
}
