#include "TimeManager.h"

// Uses ESP32 built-in SNTP (configTime / getLocalTime) instead of the
// NTPClient library. ESP-IDF handles DNS retries internally and does not
// produce serial errors when the lookup fails transiently.

void TimeManager::sync() {
    // configTime sets the TZ environment variable so that localtime_r()
    // returns the correct local time after sync.
    configTime(NTP_UTC_OFFSET + NTP_DST_OFFSET, 0, NTP_SERVER);

    // Wait up to 10 s for the first sync; SNTP continues in the background
    // regardless of whether this initial wait succeeds.
    struct tm timeinfo{};
    if (getLocalTime(&timeinfo, 10000)) {
        _synced = true;
        char buf[32];
        strftime(buf, sizeof(buf), "%H:%M:%S", &timeinfo);
        Serial.printf("[Time] Synced: %s\n", buf);
    } else {
        Serial.println("[Time] NTP sync timeout — retrying in background");
    }
}

void TimeManager::update() {
    if (_synced) return;
    // getLocalTime with 0 ms timeout returns false until SNTP has set the
    // clock for the first time — no blocking, no DNS calls from our code.
    struct tm timeinfo{};
    if (getLocalTime(&timeinfo, 0)) {
        _synced = true;
        char buf[32];
        strftime(buf, sizeof(buf), "%H:%M:%S", &timeinfo);
        Serial.printf("[Time] Synced (background): %s\n", buf);
    }
}

tm TimeManager::now() const {
    time_t epoch = time(nullptr);
    tm t{};
    localtime_r(&epoch, &t);
    return t;
}

bool TimeManager::isSynced() const {
    return _synced;
}
