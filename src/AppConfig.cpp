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
    serial_safe_printf("[AppConfig] snoozeMinutes=%u\n", _snoozeMinutes);
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
    doc["snoozeMinutes"] = _snoozeMinutes;

    File f = SD.open(APP_CONFIG_FILE, FILE_WRITE);
    if (!f) {
        serial_safe_println("[AppConfig] cannot open /config.json for writing");
        return;
    }
    serializeJson(doc, f);
    f.close();
}

void AppConfig::setSnoozeMinutes(uint16_t m) {
    if (m < 1)  m = 1;
    if (m > 60) m = 60;
    _snoozeMinutes = m;
    save();
}
