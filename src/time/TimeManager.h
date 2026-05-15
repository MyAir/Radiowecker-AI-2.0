#pragma once
#include <Arduino.h>
#include <NTPClient.h>
#include <WiFiUdp.h>
#include <time.h>
#include "../config.h"

class TimeManager {
public:
    void sync();            // initial NTP sync (call after WiFi is up)
    void update();          // call from loop() to refresh internal state

    tm   now() const;       // current local time as broken-down struct
    bool isSynced() const;

private:
    WiFiUDP   _udp;
    NTPClient _ntp{_udp, NTP_SERVER, NTP_UTC_OFFSET + NTP_DST_OFFSET, 60000};
    bool      _synced = false;
};
