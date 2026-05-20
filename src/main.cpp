#include <Arduino.h>
#include <SD.h>
#include <SPI.h>
#include <LittleFS.h>

#include "config.h"
#include "serial_safe.h"
#include "display/DisplayManager.h"
#include "display/MainScreen.h"
#include "network/NetworkManager.h"
#include "network/OtaManager.h"
#include "time/TimeManager.h"
#include "audio/AudioPlayer.h"
#include "alarm/AlarmManager.h"
#include "sensors/SensorManager.h"

// Global Serial mutex (declared extern in serial_safe.h)
SemaphoreHandle_t g_serial_mutex = nullptr;

// ---------------------------------------------------------------------------
// Module instances
// ---------------------------------------------------------------------------
DisplayManager display;
MainScreen     mainScreen;
WiFiConnector  network;
OtaManager     ota;
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
    serial_safe_begin();
    // Wait up to 5 s for the USB CDC host to enumerate so the full panic
    // trace is captured even in a fast boot-loop.  Remove after debugging.
    {
        uint32_t t0 = millis();
        while (!Serial && millis() - t0 < 5000) delay(10);
    }
    serial_safe_println("\n\n=== Radiowecker AI 2.0 ===");

    // Display + LVGL must be first
    display.begin();

    // SD card
    SPI.begin(SD_SCK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);
    if (!SD.begin(SD_CS_PIN)) {
        serial_safe_println("[Main] SD card not found");
    } else {
        serial_safe_println("[Main] SD card OK");

        // The SD card may carry a config.json left over from another
        // project on this hardware.  Move it aside so we don't accidentally
        // overwrite it, then drop a fresh empty placeholder.
        // Skip the backup if the file already belongs to this project
        // (identified by the "radiowecker2" project marker in the JSON).
        if (SD.exists("/config.json")) {
            bool ownsConfig = false;
            File probe = SD.open("/config.json", FILE_READ);
            if (probe) {
                char buf[257];
                size_t n = probe.readBytes(buf, sizeof(buf) - 1);
                buf[n] = '\0';
                probe.close();
                if (strstr(buf, "radiowecker2") != nullptr) {
                    ownsConfig = true;
                }
            }
            if (ownsConfig) {
                serial_safe_println("[Main] SD /config.json belongs to this project, keeping it");
            } else {
                String bak = "/config.json.bak";
                for (int n = 1; SD.exists(bak.c_str()) && n < 100; ++n) {
                    bak = String("/config.json.bak.") + n;
                }
                if (SD.rename("/config.json", bak.c_str())) {
                    serial_safe_printf("[Main] Existing SD /config.json backed up to %s\n",
                                       bak.c_str());
                } else {
                    serial_safe_println("[Main] WARN: could not rename SD /config.json");
                }
            }
        }
        if (!SD.exists("/config.json")) {
            File f = SD.open("/config.json", FILE_WRITE);
            if (f) {
                f.print("{\"project\":\"radiowecker2\",\"alarms\":[]}");
                f.close();
                serial_safe_println("[Main] Created fresh /config.json on SD");
            }
        }
    }

    // LittleFS (alarms, app config)
    // NOTE: partition label in partitions.csv is "littlefs", not "spiffs"
    if (!LittleFS.begin(true, "/littlefs", 10, "littlefs")) {
        serial_safe_println("[Main] LittleFS mount failed");
    } else if (!LittleFS.exists(CONFIG_FILE)) {
        // Seed an empty config so subsequent reads/writes don't log
        // "file does not exist" warnings from vfs_api.
        File f = LittleFS.open(CONFIG_FILE, "w");
        if (f) {
            f.print("{\"alarms\":[]}");
            f.close();
            serial_safe_println("[Main] Seeded LittleFS " CONFIG_FILE);
        }
    }

    // Environmental sensors (shares I2C bus with touch — Wire1)
    sensors.begin();

    // Network → NTP (falls back to captive portal if no SD credentials)
    network.connect();
    if (network.isPortalActive()) {
        display.showHotspotScreen(WIFI_AP_SSID);
    } else {
        serial_safe_println("[Main] Creating main screen...");
        mainScreen.create();
        serial_safe_println("[Main] Main screen created");
        const String wifiSSID = network.isConnected() ? WiFi.SSID() : "Not Connected";
        const String wifiIP   = network.isConnected() ? network.localIP() : "---";
        mainScreen.updateWifi(wifiSSID.c_str(), wifiIP.c_str(), wifiQuality());
    }
    if (network.isConnected()) {
        ota.onStart([]() {
            audio.stop();
            display.showOtaScreen(NET_HOSTNAME);
        });
        ota.onProgress([](uint8_t pct) {
            display.updateOtaProgress(pct);
        });
        ota.onError([](const char* msg) {
            display.showOtaError(msg);
        });
        ota.begin(NET_HOSTNAME);
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

    // OTA updates (no-op until begin() has been called)
    ota.loop();

    // While an OTA transfer is in progress, only the OTA progress UI matters.
    // Skip touch / audio / time / alarm / sensor work to free CPU and avoid
    // Wire1 contention while the panel is being repainted.
    if (ota.isUpdating()) {
        display.loop();
        return;
    }

    // Touch state — read GT911 via Wire1 here (main loop) so it never
    // conflicts with SensorManager::read() which also uses Wire1.
    display.pollTouch();

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
            mainScreen.updateSensors(r.temperature, r.humidity, r.eco2, r.tvoc);
#if LOG_SENSORS
            serial_safe_printf("[Sensors] T=%.1f°C RH=%.0f%% TVOC=%d eco2=%d light=%d\n",
                          r.temperature, r.humidity, r.tvoc, r.eco2, r.light);
#endif
        }
    }
}

