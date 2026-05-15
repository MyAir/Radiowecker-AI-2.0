#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include "../config.h"

class WiFiConnector {
public:
    // Reads credentials from CONFIG_FILE on LittleFS, then connects.
    // Blocks up to timeoutMs milliseconds.
    void connect(uint32_t timeoutMs = 20000);

    bool isConnected() const;

    // Helpers
    String localIP() const;

private:
    void _loadCredentials(String& ssid, String& password);
};
