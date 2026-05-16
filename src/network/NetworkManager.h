#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <DNSServer.h>
#include <WebServer.h>
#include <SD.h>
#include <ArduinoJson.h>
#include <vector>
#include "../config.h"

class WiFiConnector {
public:
    // Load credentials from SD card (SD_WIFI_FILE) and connect to WiFi.
    // If no credentials exist or the connection times out, a captive-portal
    // AP is started so the user can configure WiFi via a web page.
    void connect(uint32_t timeoutMs = 20000);

    // Drive captive-portal DNS + HTTP — must be called every loop().
    void loop();

    bool isConnected()    const;
    bool isPortalActive() const { return _portalActive; }
    String localIP()      const;

private:
    struct NetworkEntry { String ssid; int32_t rssi; };

    bool _loadCredentials(String& ssid, String& password);
    bool _saveCredentials(const String& ssid, const String& password);
    void _startPortal();
    void _buildNetworkOptions(String& out);

    DNSServer*  _dns          = nullptr;
    WebServer*  _server       = nullptr;
    bool        _portalActive = false;
    uint32_t    _restartAt    = 0;    // millis() timestamp for ESP.restart()
    std::vector<NetworkEntry> _networks;
};
