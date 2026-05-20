#include "OtaManager.h"
#include <ArduinoOTA.h>
#include <WiFi.h>

void OtaManager::begin(const char* hostname) {
    if (_active) return;
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[OTA] Not started — WiFi not connected");
        return;
    }

    ArduinoOTA.setHostname(hostname);
    // No password by default — set one here if desired:
    // ArduinoOTA.setPassword("changeme");

    ArduinoOTA.onStart([this]() {
        _updating = true;
        const char* type = (ArduinoOTA.getCommand() == U_FLASH) ? "sketch" : "filesystem";
        Serial.printf("[OTA] Start updating %s\n", type);
        if (_onStart) _onStart();
    });
    ArduinoOTA.onEnd([this]() {
        _updating = false;
        Serial.println("\n[OTA] End");
        if (_onEnd) _onEnd();
    });
    ArduinoOTA.onProgress([this](unsigned int progress, unsigned int total) {
        const uint8_t pct = total ? (uint8_t)((progress * 100U) / total) : 0;
        Serial.printf("[OTA] Progress: %u%%\r", (unsigned)pct);
        if (_onProgress) _onProgress(pct);
    });
    ArduinoOTA.onError([this](ota_error_t error) {
        _updating = false;
        const char* msg = "unknown";
        switch (error) {
            case OTA_AUTH_ERROR:    msg = "auth failed";    break;
            case OTA_BEGIN_ERROR:   msg = "begin failed";   break;
            case OTA_CONNECT_ERROR: msg = "connect failed"; break;
            case OTA_RECEIVE_ERROR: msg = "receive failed"; break;
            case OTA_END_ERROR:     msg = "end failed";     break;
            default: break;
        }
        Serial.printf("[OTA] Error[%u]: %s\n", error, msg);
        if (_onError) _onError(msg);
    });

    ArduinoOTA.begin();
    _active = true;
    Serial.printf("[OTA] Ready — hostname \"%s\", IP %s\n",
                  hostname, WiFi.localIP().toString().c_str());
}

void OtaManager::loop() {
    if (!_active) return;
    ArduinoOTA.handle();
}
