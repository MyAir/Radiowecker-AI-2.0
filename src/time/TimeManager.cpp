#include "TimeManager.h"

void TimeManager::sync() {
    _ntp.begin();
    _ntp.forceUpdate();
    _synced = true;
    Serial.printf("[Time] Synced: %s\n", _ntp.getFormattedTime().c_str());
}

void TimeManager::update() {
    _ntp.update();
}

tm TimeManager::now() const {
    time_t epoch = _ntp.getEpochTime();
    tm t{};
    localtime_r(&epoch, &t);
    return t;
}

bool TimeManager::isSynced() const {
    return _synced;
}
