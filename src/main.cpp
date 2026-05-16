#include <Arduino.h>
#include <SD.h>
#include <SPI.h>
#include <LittleFS.h>

#include "config.h"
#include "display/DisplayManager.h"
#include "display/MainScreen.h"
#include "network/NetworkManager.h"
#include "time/TimeManager.h"
#include "audio/AudioPlayer.h"
#include "alarm/AlarmManager.h"
#include "sensors/SensorManager.h"

// ---------------------------------------------------------------------------
// Module instances
// ---------------------------------------------------------------------------
DisplayManager display;
MainScreen     mainScreen;
WiFiConnector network;
TimeManager    timeManager;
AudioPlayer    audio;
AlarmManager   alarms;
SensorManager  sensors;

// ---------------------------------------------------------------------------
// Sensor poll interval
// ---------------------------------------------------------------------------
static constexpr uint32_t SENSOR_INTERVAL_MS = 5000;
static uint32_t s_lastSensorMs = 0;
static uint32_t s_lastUiMs     = 0;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
static int wifiQuality() {
    if (!network.isConnected()) return 0;
    const int rssi = WiFi.RSSI();
    if (rssi <= -100) return 0;
    if (rssi >= -50)  return 100;
    return 2 * (rssi + 100);
}

// ---------------------------------------------------------------------------
// setup()
// ---------------------------------------------------------------------------
void setup() {
    Serial.begin(115200);
    delay(200);
    Serial.println("\n\n=== Radiowecker AI 2.0 ===");

    // Display + LVGL must be first
    display.begin();

    // SD card
    SPI.begin(SD_SCK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);
    if (!SD.begin(SD_CS_PIN)) {
        Serial.println("[Main] SD card not found");
    } else {
        Serial.println("[Main] SD card OK");
    }

    // LittleFS (alarms, app config)
    if (!LittleFS.begin(true)) {
        Serial.println("[Main] LittleFS mount failed");
    }

    // Environmental sensors (shares I2C bus with touch — Wire1)
    sensors.begin();

    // Network → NTP (falls back to captive portal if no SD credentials)
    network.connect();
    if (network.isPortalActive()) {
        display.showHotspotScreen(WIFI_AP_SSID);
    } else {
        Serial.println("[Main] Creating main screen...");
        mainScreen.create();
        Serial.println("[Main] Main screen created");
        const String wifiSSID = network.isConnected() ? WiFi.SSID() : "Not Connected";
        const String wifiIP   = network.isConnected() ? network.localIP() : "---";
        mainScreen.updateWifi(wifiSSID.c_str(), wifiIP.c_str(), wifiQuality());
    }
    if (network.isConnected()) {
        timeManager.sync();
    }

    // Audio
    audio.begin();
    audio.setVolume(DEFAULT_VOLUME);

    // Alarm manager
    alarms.begin();
    alarms.setTriggerCallback([](const Alarm& alarm) {
        if (alarm.streamUrl.length() > 0) {
            audio.playStream(alarm.streamUrl.c_str());
        } else {
            audio.playStream(DEFAULT_STREAM);
        }
    });
}

// ---------------------------------------------------------------------------
// loop()
// ---------------------------------------------------------------------------
void loop() {
    // LVGL timer engine (must run every cycle)
    display.loop();

    // Captive portal (no-op once WiFi is connected)
    network.loop();

    // Audio streaming
    audio.loop();

    // Time updates
    timeManager.update();

    // Alarm check
    alarms.check(timeManager.now());

    // UI update — time and WiFi status (once per second)
    const uint32_t now = millis();
    if (now - s_lastUiMs >= 1000) {
        s_lastUiMs = now;
        mainScreen.updateTime(timeManager.now());
        const String wifiSSID = network.isConnected() ? WiFi.SSID() : "Not Connected";
        const String wifiIP   = network.isConnected() ? network.localIP() : "---";
        mainScreen.updateWifi(wifiSSID.c_str(), wifiIP.c_str(), wifiQuality());
    }

    // Sensor poll (rate-limited)
    if (now - s_lastSensorMs >= SENSOR_INTERVAL_MS) {
        s_lastSensorMs = now;
        const SensorManager::Reading r = sensors.read();
        if (r.valid) {
            // TODO: push sensor values to LVGL UI labels
            Serial.printf("[Sensors] T=%.1f°C RH=%.0f%% TVOC=%d eco2=%d light=%d\n",
                          r.temperature, r.humidity, r.tvoc, r.eco2, r.light);
        }
    }
}

