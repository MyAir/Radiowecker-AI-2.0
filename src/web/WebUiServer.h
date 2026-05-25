#pragma once
#include <Arduino.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <functional>

// Forward decls — keep this header light.
class AlarmManager;
class AppConfig;
class StationsList;
class WeatherManager;
class AudioPlayer;
class TimeManager;
class WiFiConnector;

/**
 * WebUiServer
 *
 * LAN-only HTTP server on port 80. Started after WiFi connects (STA mode)
 * alongside ArduinoOTA. Serves a single-page web app from SD `/www/` and
 * a small JSON REST API for:
 *   - alarms      : list / create / update / delete, master switch, skip
 *   - app config  : brightness, snooze, device name, ...
 *   - stations    : list / create / update / delete
 *   - weather     : read / write /weather.json
 *   - system      : status, NTP sync, reboot
 *
 * All HTTP work runs on the main loop (Core 1) inside `loop()`, same
 * thread as LVGL and the managers — no locking required.
 *
 * Register `onAlarmsChanged` / `onConfigChanged` from main.cpp to mirror
 * state changes onto the LVGL MainScreen (bell icon + "next alarm").
 */
class WebUiServer {
public:
    using Callback = std::function<void()>;

    void begin(AlarmManager&   alarms,
               AppConfig&      cfg,
               StationsList&   stations,
               WeatherManager& weather,
               AudioPlayer&    audio,
               TimeManager&    time,
               WiFiConnector&  net);

    /** Service pending HTTP requests + deferred reboot. Call every loop(). */
    void loop();

    void onAlarmsChanged(Callback cb) { _onAlarmsChanged = std::move(cb); }
    void onConfigChanged(Callback cb) { _onConfigChanged = std::move(cb); }

    bool isRunning() const { return _running; }

private:
    void _handle();
    bool _serveStatic(const String& uri);
    void _sendJson(int code, const String& body);
    void _sendOk();
    void _sendError(int code, const char* msg);
    bool _readJsonBody(JsonDocument& doc);
    void _addCors();

    void _apiStatus();
    void _apiSystem();
    void _apiNtpSync();
    void _apiReboot();

    void _apiAlarmsList();
    void _apiAlarmCreate();
    void _apiAlarmUpdate(uint32_t id);
    void _apiAlarmDelete(uint32_t id);
    void _apiAlarmMaster();
    void _apiAlarmSkipNext();
    void _apiAlarmUnskip();

    void _apiConfigGet();
    void _apiConfigPut();

    void _apiStationsList();
    void _apiStationCreate();
    void _apiStationUpdate(int index);
    void _apiStationDelete(int index);

    void _apiWeatherGet();
    void _apiWeatherPut();

    WebServer       _server{80};
    AlarmManager*   _alarms   = nullptr;
    AppConfig*      _cfg      = nullptr;
    StationsList*   _stations = nullptr;
    WeatherManager* _weather  = nullptr;
    AudioPlayer*    _audio    = nullptr;
    TimeManager*    _time     = nullptr;
    WiFiConnector*  _net      = nullptr;

    bool     _running    = false;
    uint32_t _rebootAtMs = 0;   // 0 = no reboot scheduled

    Callback _onAlarmsChanged;
    Callback _onConfigChanged;
};
