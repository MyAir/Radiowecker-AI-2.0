#include <Arduino.h>
#include <SD.h>
#include <SPI.h>
#include <LittleFS.h>
#include <driver/gpio.h>
#include <soc/system_reg.h>
#include <soc/soc.h>

#include "config.h"
#include "AppConfig.h"
#include "serial_safe.h"
#include "display/DisplayManager.h"
#include "display/MainScreen.h"
#include "display/SettingsScreen.h"
#include "display/AlarmSetupScreen.h"
#include "display/AlarmScreen.h"
#include "display/GeneralSettingsScreen.h"
#include "network/NetworkManager.h"
#include "network/OtaManager.h"
#include "time/TimeManager.h"
#include "audio/AudioPlayer.h"
#include "audio/StationsList.h"
#include "alarm/AlarmManager.h"
#include "sensors/SensorManager.h"
#include "weather/WeatherManager.h"

// Global Serial mutex (declared extern in serial_safe.h)
SemaphoreHandle_t g_serial_mutex = nullptr;

// ---------------------------------------------------------------------------
// Module instances
// ---------------------------------------------------------------------------
DisplayManager display;
MainScreen     mainScreen;
SettingsScreen settingsScreen;
static uint8_t s_brightness = 128;  // matches DisplayManager::begin() startup value
WiFiConnector  network;
OtaManager     ota;
TimeManager    timeManager;
AudioPlayer    audio;
StationsList   g_stations;
AlarmManager   alarms;
SensorManager  sensors;
WeatherManager weather;

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

// Short German weekday (Mo, Di, Mi, Do, Fr, Sa, So) — wday: 0=Sun..6=Sat
static const char* germanWdayShort(int wday) {
    static const char* const d[7] = {"So", "Mo", "Di", "Mi", "Do", "Fr", "Sa"};
    return (wday >= 0 && wday <= 6) ? d[wday] : "";
}

// Format the "Next alarm" string into buf. Returns true if there is one.
//   On disabled master switch or no alarms: writes "---" and returns false.
//   Today:    "Heute  06:30"
//   Tomorrow: "Morgen 06:30"
//   Else:     "Mo 02.01.2026  06:30"
static bool formatNextAlarm(char* buf, size_t bufLen) {
    if (!alarms.isMasterEnabled()) {
        snprintf(buf, bufLen, "---");
        return false;
    }
    tm now = timeManager.now();
    size_t idx = 0;
    tm fire{};
    if (!alarms.nextAlarm(now, &idx, &fire)) {
        snprintf(buf, bufLen, "---");
        return false;
    }
    // Compute "days from today" by date (yday-aware across year boundary).
    tm a = now;  a.tm_hour = 0; a.tm_min = 0; a.tm_sec = 0;
    tm b = fire; b.tm_hour = 0; b.tm_min = 0; b.tm_sec = 0;
    time_t ta = mktime(&a), tb = mktime(&b);
    long days = (ta >= 0 && tb >= 0) ? (long)((tb - ta) / 86400) : 99;
    if (days == 0) {
        snprintf(buf, bufLen, "Heute  %02d:%02d", fire.tm_hour, fire.tm_min);
    } else if (days == 1) {
        snprintf(buf, bufLen, "Morgen  %02d:%02d", fire.tm_hour, fire.tm_min);
    } else {
        snprintf(buf, bufLen, "%s %02d.%02d.%04d  %02d:%02d",
                 germanWdayShort(fire.tm_wday),
                 fire.tm_mday, fire.tm_mon + 1, fire.tm_year + 1900,
                 fire.tm_hour, fire.tm_min);
    }
    return true;
}

static void refreshNextAlarmLabel() {
    char buf[40];
    formatNextAlarm(buf, sizeof(buf));
    mainScreen.setNextAlarm(buf);
}

// ---------------------------------------------------------------------------
// setup()
// ---------------------------------------------------------------------------
void setup() {
    Serial.begin(115200);
    serial_safe_begin();
    // Disable the USB Serial/JTAG controller so it releases GPIO 19/20.
    // Those pins are wired to the native-USB connector but we re-use them
    // for the I2S audio output (BCLK=20, LRCLK=19). While USB-JTAG is
    // active it keeps driving those pads, so i2s_write() never drains its
    // DMA buffers and the audio task hangs until the task watchdog fires.
    // The ROM bootloader still runs USB-JTAG before our code starts, so
    // flashing over native USB (BOOT+RESET) keeps working.
    REG_SET_BIT(SYSTEM_PERIP_RST_EN0_REG, SYSTEM_USB_DEVICE_RST);
    REG_CLR_BIT(SYSTEM_PERIP_CLK_EN0_REG, SYSTEM_USB_DEVICE_CLK_EN);
    // Detach pins 19/20 from any peripheral and leave them high-Z so the
    // I2S GPIO matrix can drive them once AudioOutputI2S::SetPinout runs.
    gpio_reset_pin((gpio_num_t)19);
    gpio_reset_pin((gpio_num_t)20);
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
        // Alarm master toggle — tapping the bell flips the switch and persists it
        mainScreen.setOnAlarmToggle([]() {
            alarms.setMasterEnabled(!alarms.isMasterEnabled());
            mainScreen.setAlarmEnabled(alarms.isMasterEnabled());
            refreshNextAlarmLabel();
            serial_safe_printf("[Main] Alarm master: %s\n",
                               alarms.isMasterEnabled() ? "ON" : "OFF");
        });
        // Skip next upcoming alarm (Next button)
        mainScreen.setOnSkipAlarm([]() {
            alarms.skipNext();
            refreshNextAlarmLabel();
            serial_safe_println("[Main] Alarm skipNext");
        });
        // Undo last skip (Prev button)
        mainScreen.setOnPrevAlarm([]() {
            alarms.unskip();
            refreshNextAlarmLabel();
            serial_safe_println("[Main] Alarm unskip");
        });
        // Cogwheel — open settings screen with slide-from-left animation
        mainScreen.setOnSettings([]() {
            // Open the alarm CRUD screen. SettingsScreen has already torn
            // itself down (see _detachForChildScreen); AlarmSetupScreen's
            // create() uses auto_del=true to make LVGL delete the old
            // settings screen after the slide animation completes.
            settingsScreen.setOnOpenAlarms([]() {
                alarmSetupScreen.setOnPreviewStream([](const char* url) {
                    audio.playStream(url);
                });
                alarmSetupScreen.setOnPreviewFile([](const char* path) {
                    audio.playFile(path);
                });
                alarmSetupScreen.setOnStop([]() {
                    audio.stop();
                });
                alarmSetupScreen.setOnVolumeChange([](uint8_t vol) {
                    audio.setVolume(vol);
                });
                alarmSetupScreen.setOnChanged([]() {
                    refreshNextAlarmLabel();
                });
                alarmSetupScreen.create(mainScreen.screen());
            });
            settingsScreen.setOnOpenGeneral([]() {
                generalSettingsScreen.setOnBrightnessChange([](uint8_t br) {
                    s_brightness = br;
                    display.setBrightness(br);
                });
                generalSettingsScreen.setOnTestAlarm([](size_t idx) {
                    const Alarm* a = alarms.at(idx);
                    if (!a) return;
                    audio.setVolume(a->volume);
                    if (a->soundType == SoundType::SD && a->soundPath.length() > 0)
                        audio.playFile(a->soundPath.c_str());
                    else if (a->streamUrl.length() > 0)
                        audio.playStream(a->streamUrl.c_str());
                    else
                        audio.playStream(DEFAULT_STREAM);
                    if (!alarmScreen.isVisible())
                        alarmScreen.show(mainScreen.screen(), *a, s_brightness);
                });
                // Build newline-separated alarm option list for the debug dropdown
                static char s_alarmOpts[640];
                s_alarmOpts[0] = '\0';
                const auto& als = alarms.alarms();
                for (size_t i = 0; i < als.size(); ++i) {
                    char line[80];
                    snprintf(line, sizeof(line), "%02d:%02d  %s",
                             als[i].hour, als[i].minute, als[i].title.c_str());
                    if (i > 0) strncat(s_alarmOpts, "\n",
                                       sizeof(s_alarmOpts) - strlen(s_alarmOpts) - 1);
                    strncat(s_alarmOpts, line,
                            sizeof(s_alarmOpts) - strlen(s_alarmOpts) - 1);
                }
                generalSettingsScreen.create(mainScreen.screen(), s_brightness,
                                             als.empty() ? nullptr : s_alarmOpts);
            });
            settingsScreen.create(mainScreen.screen());
        });
        const String wifiSSID = network.isConnected() ? WiFi.SSID() : "Not Connected";
        const String wifiIP   = network.isConnected() ? network.localIP() : "---";
        mainScreen.updateWifi(wifiSSID.c_str(), wifiIP.c_str(), wifiQuality());
    }
    // Weather (reads /weather.json from SD; needs SD + WiFi to actually poll)
    weather.begin();

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

    // App config (snooze duration, etc.) — reads SD /config.json
    g_appConfig.load();

    // Stations list — reads SD /stations.json
    g_stations.load();

    // Alarm manager (reads SD /alarms.json, seeds defaults if missing)
    alarms.begin();
    // Sync bell icon to loaded state (masterEnabled may be false from disk)
    mainScreen.setAlarmEnabled(alarms.isMasterEnabled());
    alarms.setTriggerCallback([](const Alarm& alarm) {
        audio.setVolume(alarm.volume);
        if (alarm.soundType == SoundType::SD && alarm.soundPath.length() > 0) {
            audio.playFile(alarm.soundPath.c_str());
        } else if (alarm.streamUrl.length() > 0) {
            audio.playStream(alarm.streamUrl.c_str());
        } else {
            audio.playStream(DEFAULT_STREAM);
        }
        // Bring up the alarm-firing screen (boosts brightness to full,
        // overlays MainScreen, snooze/stop buttons handle audio.stop()).
        if (!alarmScreen.isVisible()) {
            alarmScreen.show(mainScreen.screen(), alarm, s_brightness);
        }
    });

    // AlarmScreen callbacks (wired once; the instance is reused).
    alarmScreen.setOnBrightness([](uint8_t br) {
        display.setBrightness(br);
    });
    alarmScreen.setOnStop([]() {
        audio.stop();
    });
    alarmScreen.setOnSnoozeFire([](const Alarm& a) {
        // Re-fire the alarm action after the snooze interval elapses.
        audio.setVolume(a.volume);
        if (a.soundType == SoundType::SD && a.soundPath.length() > 0) {
            audio.playFile(a.soundPath.c_str());
        } else if (a.streamUrl.length() > 0) {
            audio.playStream(a.streamUrl.c_str());
        } else {
            audio.playStream(DEFAULT_STREAM);
        }
        alarmScreen.show(mainScreen.screen(), a, s_brightness);
    });
    // Initial paint of the "Next alarm" line.
    refreshNextAlarmLabel();
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
        refreshNextAlarmLabel();
        if (alarmScreen.isVisible()) {
            alarmScreen.tick(timeManager.now(), weather, audio);
        }
    }

    // Weather poll (every 5 min when WiFi is up)
    weather.loop();

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

