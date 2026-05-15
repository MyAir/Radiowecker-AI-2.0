#include "NetworkManager.h"

void WiFiConnector::connect(uint32_t timeoutMs) {
    if (!LittleFS.begin(true)) {
        Serial.println("[Network] LittleFS mount failed");
        return;
    }

    String ssid, password;
    _loadCredentials(ssid, password);

    if (ssid.isEmpty()) {
        Serial.println("[Network] No SSID in config — skipping WiFi");
        return;
    }

    Serial.printf("[Network] Connecting to %s …\n", ssid.c_str());
    WiFi.begin(ssid.c_str(), password.c_str());

    uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - start < timeoutMs) {
        delay(250);
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.printf("[Network] Connected, IP: %s\n", WiFi.localIP().toString().c_str());
    } else {
        Serial.println("[Network] Connection timed out");
    }
}

bool WiFiConnector::isConnected() const {
    return WiFi.status() == WL_CONNECTED;
}

String WiFiConnector::localIP() const {
    return WiFi.localIP().toString();
}

void WiFiConnector::_loadCredentials(String& ssid, String& password) {
    if (!LittleFS.exists(CONFIG_FILE)) {
        Serial.println("[Network] config.json not found");
        return;
    }

    File f = LittleFS.open(CONFIG_FILE, "r");
    if (!f) return;

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, f);
    f.close();

    if (err) {
        Serial.printf("[Network] JSON parse error: %s\n", err.c_str());
        return;
    }

    ssid     = doc["wifi"]["ssid"]     | "";
    password = doc["wifi"]["password"] | "";
}
