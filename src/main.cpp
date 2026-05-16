#include <Arduino.h>
#include <SD.h>
#include <SPI.h>
#include <LittleFS.h>

#include "config.h"
#include "display/DisplayManager.h"
#include "network/NetworkManager.h"
#include "time/TimeManager.h"
#include "audio/AudioPlayer.h"
#include "alarm/AlarmManager.h"
#include "sensors/SensorManager.h"

// ---------------------------------------------------------------------------
// Module instances
// ---------------------------------------------------------------------------
DisplayManager display;
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

    // NOTE: EEZ Studio–generated UI initialisation goes here, e.g.:
    // ui_init();
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

    // Sensor poll (rate-limited)
    const uint32_t now = millis();
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

