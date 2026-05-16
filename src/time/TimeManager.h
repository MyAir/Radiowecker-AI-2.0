#pragma once
#include <Arduino.h>
#include <time.h>
#include "../config.h"

class TimeManager {
public:
    void sync();            // initial NTP sync via configTime() (call after WiFi is up)
    void update();          // call from loop() — detects background sync completion

    tm   now() const;       // current local time as broken-down struct
    bool isSynced() const;

private:
    bool _synced = false;
};
