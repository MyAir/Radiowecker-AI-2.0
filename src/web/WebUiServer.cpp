#include "WebUiServer.h"

#include <SD.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <detail/RequestHandler.h>

// Catch-all RequestHandler: matches every URI+method.  Registered via
// addHandler() so arduino-esp32's WebServer never falls through to the
// "request handler not found" log_e() path.  All routing is done inside
// the callback (WebUiServer::_handle()).
class WildcardHandler : public RequestHandler {
public:
    using Fn       = std::function<void()>;
    using UploadFn = std::function<void(HTTPUpload&)>;
    WildcardHandler(Fn fn, UploadFn up) : _fn(std::move(fn)), _up(std::move(up)) {}
    bool canHandle(HTTPMethod, String) override { return true; }
    bool canUpload(String uri) override { return uri == "/api/sd/upload"; }
    bool handle(WebServer&, HTTPMethod, String) override { _fn(); return true; }
    void upload(WebServer&, String, HTTPUpload& u) override { _up(u); }
private:
    Fn       _fn;
    UploadFn _up;
};

#include "../serial_safe.h"
#include "../alarm/AlarmManager.h"
#include "../AppConfig.h"
#include "../audio/StationsList.h"
#include "../audio/AudioPlayer.h"
#include "../weather/WeatherManager.h"
#include "../time/TimeManager.h"
#include "../network/NetworkManager.h"

// ---------------------------------------------------------------------------
// Static helpers
// ---------------------------------------------------------------------------
static const char* mimeFor(const String& path) {
    if (path.endsWith(".html") || path.endsWith(".htm")) return "text/html; charset=utf-8";
    if (path.endsWith(".css"))                           return "text/css; charset=utf-8";
    if (path.endsWith(".js"))                            return "application/javascript; charset=utf-8";
    if (path.endsWith(".json"))                          return "application/json; charset=utf-8";
    if (path.endsWith(".png"))                           return "image/png";
    if (path.endsWith(".ico"))                           return "image/x-icon";
    if (path.endsWith(".svg"))                           return "image/svg+xml";
    return "application/octet-stream";
}

static int wifiQuality(int rssi) {
    if (rssi <= -100) return 0;
    if (rssi >= -50)  return 100;
    return 2 * (rssi + 100);
}

static const char* germanWdayShort(int wday) {
    static const char* const d[7] = {"So", "Mo", "Di", "Mi", "Do", "Fr", "Sa"};
    return (wday >= 0 && wday <= 6) ? d[wday] : "";
}

// Same formatting as main.cpp's formatNextAlarm() — duplicated here so the
// web layer doesn't have to reach back into main.cpp.
static bool formatNextAlarm(const AlarmManager& alarms, const tm& now,
                            char* buf, size_t bufLen, time_t* outEpoch) {
    if (outEpoch) *outEpoch = 0;
    if (!alarms.isMasterEnabled()) { snprintf(buf, bufLen, "---"); return false; }
    size_t idx = 0; tm fire{};
    if (!alarms.nextAlarm(now, &idx, &fire)) { snprintf(buf, bufLen, "---"); return false; }
    tm a = now;  a.tm_hour = 0; a.tm_min = 0; a.tm_sec = 0;
    tm b = fire; b.tm_hour = 0; b.tm_min = 0; b.tm_sec = 0;
    time_t ta = mktime(&a), tb = mktime(&b);
    long days = (ta >= 0 && tb >= 0) ? (long)((tb - ta) / 86400) : 99;
    if (days == 0)      snprintf(buf, bufLen, "Heute  %02d:%02d", fire.tm_hour, fire.tm_min);
    else if (days == 1) snprintf(buf, bufLen, "Morgen  %02d:%02d", fire.tm_hour, fire.tm_min);
    else snprintf(buf, bufLen, "%s %02d.%02d.%04d  %02d:%02d",
                  germanWdayShort(fire.tm_wday),
                  fire.tm_mday, fire.tm_mon + 1, fire.tm_year + 1900,
                  fire.tm_hour, fire.tm_min);
    if (outEpoch) {
        tm copy = fire;
        *outEpoch = mktime(&copy);
    }
    return true;
}

// ---------------------------------------------------------------------------
// begin() / loop()
// ---------------------------------------------------------------------------
void WebUiServer::begin(AlarmManager& alarms, AppConfig& cfg,
                        StationsList& stations, WeatherManager& weather,
                        AudioPlayer& audio, TimeManager& time,
                        WiFiConnector& net) {
    _alarms   = &alarms;
    _cfg      = &cfg;
    _stations = &stations;
    _weather  = &weather;
    _audio    = &audio;
    _time     = &time;
    _net      = &net;

    // Route everything through one dispatcher. arduino-esp32 WebServer
    // doesn't support URL templates, so we parse the URI ourselves.
    // Use addHandler() with a wildcard RequestHandler (instead of
    // onNotFound) so the noisy "[E] request handler not found" log line
    // never fires.
    _server.addHandler(new WildcardHandler(
        [this]() { _handle(); },
        [this](HTTPUpload& up) { _onSdUpload(up); }));

    _server.begin();
    _running = true;
    serial_safe_printf("[Web] HTTP server listening on http://%s/\n",
                       WiFi.localIP().toString().c_str());
}

void WebUiServer::loop() {
    if (!_running) return;
    _server.handleClient();

    if (_rebootAtMs != 0 && (int32_t)(millis() - _rebootAtMs) >= 0) {
        serial_safe_println("[Web] reboot requested via API — restarting");
        delay(50);
        ESP.restart();
    }
}

// ---------------------------------------------------------------------------
// Dispatcher
// ---------------------------------------------------------------------------
void WebUiServer::_handle() {
    const HTTPMethod m = _server.method();
    String uri = _server.uri();

    // CORS preflight — answer everywhere.
    if (m == HTTP_OPTIONS) {
        _addCors();
        _server.sendHeader("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
        _server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
        _server.send(204);
        return;
    }

    // ---- API routes ----
    if (uri.startsWith("/api/")) {
        // Fixed paths
        if (uri == "/api/status"            && m == HTTP_GET)  return _apiStatus();
        if (uri == "/api/system"            && m == HTTP_GET)  return _apiSystem();
        if (uri == "/api/system/ntp-sync"   && m == HTTP_POST) return _apiNtpSync();
        if (uri == "/api/system/reboot"     && m == HTTP_POST) return _apiReboot();

        if (uri == "/api/alarms"            && m == HTTP_GET)  return _apiAlarmsList();
        if (uri == "/api/alarms"            && m == HTTP_POST) return _apiAlarmCreate();
        if (uri == "/api/alarms/master"     && m == HTTP_POST) return _apiAlarmMaster();
        if (uri == "/api/alarms/skip-next"  && m == HTTP_POST) return _apiAlarmSkipNext();
        if (uri == "/api/alarms/unskip"     && m == HTTP_POST) return _apiAlarmUnskip();

        if (uri == "/api/config"            && m == HTTP_GET)  return _apiConfigGet();
        if (uri == "/api/config"            && m == HTTP_PUT)  return _apiConfigPut();

        if (uri == "/api/stations"          && m == HTTP_GET)  return _apiStationsList();
        if (uri == "/api/stations"          && m == HTTP_POST) return _apiStationCreate();

        if (uri == "/api/files/music"       && m == HTTP_GET)  return _apiFilesMusic();

        if (uri == "/api/sd/list"           && m == HTTP_GET)    return _apiSdList();
        if (uri == "/api/sd/download"       && m == HTTP_GET)    return _apiSdDownload();
        if (uri == "/api/sd/upload"         && m == HTTP_POST)   return _apiSdUploadDone();
        if (uri == "/api/sd/file"           && m == HTTP_DELETE) return _apiSdDelete();
        if (uri == "/api/sd/mkdir"          && m == HTTP_POST)   return _apiSdMkdir();
        if (uri == "/api/sd/rename"         && m == HTTP_POST)   return _apiSdRename();
        if (uri == "/api/sd/move"           && m == HTTP_POST)   return _apiSdMove();
        if (uri == "/api/sd/play"           && m == HTTP_POST)   return _apiSdPlay();
        if (uri == "/api/sd/stop"           && m == HTTP_POST)   return _apiSdStop();

        if (uri == "/api/weather"           && m == HTTP_GET)  return _apiWeatherGet();
        if (uri == "/api/weather"           && m == HTTP_PUT)  return _apiWeatherPut();

        if (uri == "/api/geocode"           && m == HTTP_GET)  return _apiGeocode();
        if (uri == "/api/geocode/reverse"   && m == HTTP_GET)  return _apiGeocodeReverse();

        // /api/alarms/{id}
        if (uri.startsWith("/api/alarms/")) {
            const String rest = uri.substring(strlen("/api/alarms/"));
            const uint32_t id = (uint32_t)rest.toInt();
            if (id == 0) return _sendError(400, "invalid id");
            if (m == HTTP_PUT)    return _apiAlarmUpdate(id);
            if (m == HTTP_DELETE) return _apiAlarmDelete(id);
        }
        // /api/stations/{index}
        if (uri.startsWith("/api/stations/")) {
            const String rest = uri.substring(strlen("/api/stations/"));
            const int idx = rest.toInt();
            if (m == HTTP_PUT)    return _apiStationUpdate(idx);
            if (m == HTTP_DELETE) return _apiStationDelete(idx);
        }

        return _sendError(404, "no such API route");
    }

    // ---- Static files ----
    if (m == HTTP_GET) {
        if (_serveStatic(uri)) return;
    }
    _sendError(404, "not found");
}

// ---------------------------------------------------------------------------
// Static file serving (SD /www/...)
// ---------------------------------------------------------------------------
bool WebUiServer::_serveStatic(const String& uri) {
    String path = uri;
    if (path == "/" || path.length() == 0) path = "/index.html";
    // Block path traversal.
    if (path.indexOf("..") >= 0) return false;

    String full = String("/www") + path;
    if (!SD.exists(full.c_str())) return false;
    File f = SD.open(full.c_str(), FILE_READ);
    if (!f || f.isDirectory()) { if (f) f.close(); return false; }

    _addCors();
    _server.sendHeader("Cache-Control", "no-cache");
    _server.streamFile(f, mimeFor(path));
    f.close();
    return true;
}

// ---------------------------------------------------------------------------
// Response helpers
// ---------------------------------------------------------------------------
void WebUiServer::_addCors() {
    _server.sendHeader("Access-Control-Allow-Origin", "*");
}

void WebUiServer::_sendJson(int code, const String& body) {
    _addCors();
    _server.send(code, "application/json; charset=utf-8", body);
}

void WebUiServer::_sendOk() {
    _sendJson(200, "{\"ok\":true}");
}

void WebUiServer::_sendError(int code, const char* msg) {
    JsonDocument doc;
    doc["error"] = msg ? msg : "error";
    String body; serializeJson(doc, body);
    _sendJson(code, body);
}

bool WebUiServer::_readJsonBody(JsonDocument& doc) {
    const String& body = _server.arg("plain");
    DeserializationError err = deserializeJson(doc, body);
    if (err) {
        serial_safe_printf("[Web] bad JSON body: %s\n", err.c_str());
        _sendError(400, "invalid JSON body");
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// /api/status
// ---------------------------------------------------------------------------
void WebUiServer::_apiStatus() {
    JsonDocument doc;
    doc["hostname"] = _cfg->deviceName();
    doc["ssid"]     = WiFi.isConnected() ? WiFi.SSID() : String("");
    doc["ip"]       = WiFi.isConnected() ? WiFi.localIP().toString() : String("");
    const int rssi  = WiFi.isConnected() ? WiFi.RSSI() : 0;
    doc["rssi"]     = rssi;
    doc["quality"]  = WiFi.isConnected() ? wifiQuality(rssi) : 0;
    doc["heapFree"]  = (uint32_t)ESP.getFreeHeap();
    doc["psramFree"] = (uint32_t)ESP.getFreePsram();
    doc["uptimeMs"]  = (uint32_t)millis();

    const tm now = _time->now();
    char tbuf[24];
    strftime(tbuf, sizeof(tbuf), "%Y-%m-%dT%H:%M:%S", &now);
    doc["timeISO"] = tbuf;
    doc["timeSynced"] = _time->isSynced();

    JsonObject au = doc["audio"].to<JsonObject>();
    au["playing"] = _audio->isPlaying();
    au["volume"]  = _audio->volume();
    char meta[AUDIO_META_MAX * 2];
    _audio->metadata(meta, sizeof(meta));
    au["meta"] = meta;

    JsonObject al = doc["alarms"].to<JsonObject>();
    al["masterEnabled"] = _alarms->isMasterEnabled();
    char nbuf[40]; time_t epoch = 0;
    formatNextAlarm(*_alarms, now, nbuf, sizeof(nbuf), &epoch);
    al["nextLabel"] = nbuf;
    al["nextEpoch"] = (uint32_t)epoch;

    String body; serializeJson(doc, body);
    _sendJson(200, body);
}

// ---------------------------------------------------------------------------
// /api/system
// ---------------------------------------------------------------------------
void WebUiServer::_apiSystem() {
    JsonDocument doc;
    doc["build"]      = __DATE__ " " __TIME__;
    doc["sketchMD5"]  = ESP.getSketchMD5();
    doc["sdkVersion"] = ESP.getSdkVersion();
    doc["chipModel"]  = ESP.getChipModel();
    doc["cpuFreqMhz"] = (int)ESP.getCpuFreqMHz();
    doc["flashSize"]  = (uint32_t)ESP.getFlashChipSize();
    doc["heapFree"]   = (uint32_t)ESP.getFreeHeap();
    doc["heapMin"]    = (uint32_t)ESP.getMinFreeHeap();
    doc["psramFree"]  = (uint32_t)ESP.getFreePsram();
    doc["uptimeMs"]   = (uint32_t)millis();
    String body; serializeJson(doc, body);
    _sendJson(200, body);
}

void WebUiServer::_apiNtpSync() {
    _time->sync();
    _sendOk();
}

void WebUiServer::_apiReboot() {
    _sendOk();
    _rebootAtMs = millis() + 1000;  // give the response time to flush
}

// ---------------------------------------------------------------------------
// Alarms
// ---------------------------------------------------------------------------
static void alarmToJson(const Alarm& a, JsonObject obj) {
    obj["id"]        = a.id;
    obj["title"]     = a.title;
    obj["hour"]      = a.hour;
    obj["minute"]    = a.minute;
    obj["weekdays"]  = a.weekdays;
    obj["enabled"]   = a.enabled;
    obj["soundType"] = (a.soundType == SoundType::SD) ? "sd" : "stream";
    obj["streamUrl"] = a.streamUrl;
    obj["soundPath"] = a.soundPath;
    obj["volume"]    = a.volume;
}

// Parse an alarm from JSON. Returns nullptr-style error message, or nullptr on success.
static const char* alarmFromJson(const JsonObject& obj, Alarm& out) {
    if (obj["hour"].is<int>())   out.hour   = (uint8_t)((int)obj["hour"]);
    if (obj["minute"].is<int>()) out.minute = (uint8_t)((int)obj["minute"]);
    if (out.hour > 23)  return "hour out of range (0..23)";
    if (out.minute > 59) return "minute out of range (0..59)";

    if (obj["title"].is<const char*>())   out.title    = (const char*)obj["title"];
    if (obj["weekdays"].is<int>())        out.weekdays = (uint8_t)((int)obj["weekdays"]) & 0x7F;
    if (obj["enabled"].is<bool>())        out.enabled  = obj["enabled"];
    if (obj["streamUrl"].is<const char*>()) out.streamUrl = (const char*)obj["streamUrl"];
    if (obj["soundPath"].is<const char*>()) out.soundPath = (const char*)obj["soundPath"];
    if (obj["volume"].is<int>()) {
        int v = obj["volume"];
        if (v < 0 || v > 21) return "volume out of range (0..21)";
        out.volume = (uint8_t)v;
    }
    if (obj["soundType"].is<const char*>()) {
        const char* st = obj["soundType"];
        if (strcmp(st, "sd") == 0)          out.soundType = SoundType::SD;
        else if (strcmp(st, "stream") == 0) out.soundType = SoundType::Stream;
        else return "soundType must be 'sd' or 'stream'";
    }
    return nullptr;
}

void WebUiServer::_apiAlarmsList() {
    JsonDocument doc;
    doc["masterEnabled"] = _alarms->isMasterEnabled();
    JsonArray arr = doc["alarms"].to<JsonArray>();
    for (const Alarm& a : _alarms->alarms()) alarmToJson(a, arr.add<JsonObject>());
    String body; serializeJson(doc, body);
    _sendJson(200, body);
}

void WebUiServer::_apiAlarmCreate() {
    JsonDocument doc;
    if (!_readJsonBody(doc)) return;
    Alarm a;  // defaults
    JsonObject obj = doc.as<JsonObject>();
    const char* err = alarmFromJson(obj, a);
    if (err) return _sendError(400, err);
    _alarms->addAlarm(a);  // assigns id
    if (_onAlarmsChanged) _onAlarmsChanged();
    // Return the created alarm (last entry).
    JsonDocument out;
    alarmToJson(_alarms->alarms().back(), out.to<JsonObject>());
    String body; serializeJson(out, body);
    _sendJson(201, body);
}

void WebUiServer::_apiAlarmUpdate(uint32_t id) {
    JsonDocument doc;
    if (!_readJsonBody(doc)) return;
    const auto& list = _alarms->alarms();
    size_t idx = (size_t)-1;
    for (size_t i = 0; i < list.size(); ++i) if (list[i].id == id) { idx = i; break; }
    if (idx == (size_t)-1) return _sendError(404, "alarm id not found");
    Alarm a = list[idx];  // start from current
    const char* err = alarmFromJson(doc.as<JsonObject>(), a);
    if (err) return _sendError(400, err);
    _alarms->updateAlarm(idx, a);
    if (_onAlarmsChanged) _onAlarmsChanged();
    JsonDocument out;
    alarmToJson(_alarms->alarms()[idx], out.to<JsonObject>());
    String body; serializeJson(out, body);
    _sendJson(200, body);
}

void WebUiServer::_apiAlarmDelete(uint32_t id) {
    const auto& list = _alarms->alarms();
    size_t idx = (size_t)-1;
    for (size_t i = 0; i < list.size(); ++i) if (list[i].id == id) { idx = i; break; }
    if (idx == (size_t)-1) return _sendError(404, "alarm id not found");
    _alarms->removeAlarm(idx);
    if (_onAlarmsChanged) _onAlarmsChanged();
    _sendOk();
}

void WebUiServer::_apiAlarmMaster() {
    JsonDocument doc;
    if (!_readJsonBody(doc)) return;
    if (!doc["enabled"].is<bool>()) return _sendError(400, "missing 'enabled' bool");
    _alarms->setMasterEnabled(doc["enabled"]);
    if (_onAlarmsChanged) _onAlarmsChanged();
    _sendOk();
}

void WebUiServer::_apiAlarmSkipNext() {
    _alarms->skipNext();
    if (_onAlarmsChanged) _onAlarmsChanged();
    _sendOk();
}

void WebUiServer::_apiAlarmUnskip() {
    _alarms->unskip();
    if (_onAlarmsChanged) _onAlarmsChanged();
    _sendOk();
}

// ---------------------------------------------------------------------------
// AppConfig
// ---------------------------------------------------------------------------
void WebUiServer::_apiConfigGet() {
    JsonDocument doc;
    doc["snoozeMinutes"]            = _cfg->snoozeMinutes();
    doc["mainBrightness"]           = _cfg->mainBrightness();
    doc["alarmBrightness"]          = _cfg->alarmBrightness();
    doc["settingsBrightness"]       = _cfg->settingsBrightness();
    doc["maxAlarmDurationMinutes"]  = _cfg->maxAlarmDurationMinutes();
    doc["inactivityTimeoutSeconds"] = _cfg->inactivityTimeoutSeconds();
    doc["debugEnabled"]             = _cfg->debugEnabled();
    doc["alarmFallbackEnabled"]     = _cfg->alarmFallbackEnabled();
    doc["alarmFallbackPath"]        = _cfg->alarmFallbackPath();
    doc["deviceName"]               = _cfg->deviceName();
    doc["temperatureOffset"]         = _cfg->temperatureOffset();
    doc["humidityOffset"]            = _cfg->humidityOffset();
    String body; serializeJson(doc, body);
    _sendJson(200, body);
}

void WebUiServer::_apiConfigPut() {
    JsonDocument doc;
    if (!_readJsonBody(doc)) return;
    if (doc["snoozeMinutes"].is<int>())            _cfg->setSnoozeMinutes((uint16_t)(int)doc["snoozeMinutes"]);
    if (doc["mainBrightness"].is<int>())           _cfg->setMainBrightness((uint8_t)(int)doc["mainBrightness"]);
    if (doc["alarmBrightness"].is<int>())          _cfg->setAlarmBrightness((uint8_t)(int)doc["alarmBrightness"]);
    if (doc["settingsBrightness"].is<int>())       _cfg->setSettingsBrightness((uint8_t)(int)doc["settingsBrightness"]);
    if (doc["maxAlarmDurationMinutes"].is<int>())  _cfg->setMaxAlarmDurationMinutes((uint16_t)(int)doc["maxAlarmDurationMinutes"]);
    if (doc["inactivityTimeoutSeconds"].is<int>()) _cfg->setInactivityTimeoutSeconds((uint16_t)(int)doc["inactivityTimeoutSeconds"]);
    if (doc["debugEnabled"].is<bool>())            _cfg->setDebugEnabled(doc["debugEnabled"]);
    if (doc["alarmFallbackEnabled"].is<bool>())    _cfg->setAlarmFallbackEnabled(doc["alarmFallbackEnabled"]);
    if (doc["alarmFallbackPath"].is<const char*>()) _cfg->setAlarmFallbackPath(String((const char*)doc["alarmFallbackPath"]));
    if (doc["deviceName"].is<const char*>())       _cfg->setDeviceName(String((const char*)doc["deviceName"]));
    if (doc["temperatureOffset"].is<float>())      _cfg->setTemperatureOffset(doc["temperatureOffset"].as<float>());
    if (doc["humidityOffset"].is<float>())         _cfg->setHumidityOffset(doc["humidityOffset"].as<float>());
    if (_onConfigChanged) _onConfigChanged();
    _apiConfigGet();  // echo back the (possibly clamped) state
}

// ---------------------------------------------------------------------------
// Stations
// ---------------------------------------------------------------------------
static void stationToJson(const StationsList::Station& s, JsonObject obj) {
    obj["name"]     = s.name;
    obj["url"]      = s.url;
    obj["favorite"] = s.favorite;
}

static const char* stationFromJson(const JsonObject& obj, StationsList::Station& s) {
    if (obj["name"].is<const char*>()) s.name = (const char*)obj["name"];
    if (obj["url"].is<const char*>())  s.url  = (const char*)obj["url"];
    if (obj["favorite"].is<bool>())    s.favorite = obj["favorite"];
    if (s.name.length() == 0) return "name required";
    if (s.url.length()  == 0) return "url required";
    return nullptr;
}

void WebUiServer::_apiStationsList() {
    JsonDocument doc;
    JsonArray arr = doc["stations"].to<JsonArray>();
    for (const auto& s : _stations->stations()) stationToJson(s, arr.add<JsonObject>());
    String body; serializeJson(doc, body);
    _sendJson(200, body);
}

void WebUiServer::_apiStationCreate() {
    JsonDocument doc;
    if (!_readJsonBody(doc)) return;
    StationsList::Station s;
    const char* err = stationFromJson(doc.as<JsonObject>(), s);
    if (err) return _sendError(400, err);
    _stations->addStation(s);
    if (!_stations->save()) return _sendError(500, "save failed");
    JsonDocument out; stationToJson(_stations->stations().back(), out.to<JsonObject>());
    String body; serializeJson(out, body);
    _sendJson(201, body);
}

void WebUiServer::_apiStationUpdate(int index) {
    if (index < 0 || (size_t)index >= _stations->stations().size())
        return _sendError(404, "station index out of range");
    JsonDocument doc;
    if (!_readJsonBody(doc)) return;
    StationsList::Station s = _stations->stations()[index];
    const char* err = stationFromJson(doc.as<JsonObject>(), s);
    if (err) return _sendError(400, err);
    _stations->updateStation((size_t)index, s);
    if (!_stations->save()) return _sendError(500, "save failed");
    JsonDocument out; stationToJson(_stations->stations()[index], out.to<JsonObject>());
    String body; serializeJson(out, body);
    _sendJson(200, body);
}

void WebUiServer::_apiStationDelete(int index) {
    if (index < 0 || (size_t)index >= _stations->stations().size())
        return _sendError(404, "station index out of range");
    _stations->removeStation((size_t)index);
    if (!_stations->save()) return _sendError(500, "save failed");
    _sendOk();
}

// ---------------------------------------------------------------------------
// Files — SD MP3 listing
// ---------------------------------------------------------------------------

// Same rules as the on-device dropdowns (AlarmSetupScreen / GeneralSettingsScreen).
static void scanMp3(const char* dir, JsonArray out, int depth, size_t maxFiles) {
    if (depth < 0 || out.size() >= maxFiles) return;
    File root = SD.open(dir);
    if (!root || !root.isDirectory()) { if (root) root.close(); return; }

    File f = root.openNextFile();
    while (f && out.size() < maxFiles) {
        const char* base = f.name();   // basename only in the ESP32 SD lib
        if (base[0] != '.') {
            const String fullPath = (strcmp(dir, "/") == 0)
                ? String("/") + base
                : String(dir) + "/" + base;
            if (f.isDirectory()) {
                scanMp3(fullPath.c_str(), out, depth - 1, maxFiles);
            } else {
                String lower = fullPath;
                lower.toLowerCase();
                if (lower.endsWith(".mp3")) out.add(fullPath);
            }
        }
        f.close();
        f = root.openNextFile();
    }
    if (f) f.close();
    root.close();
}

void WebUiServer::_apiFilesMusic() {
    JsonDocument doc;
    JsonArray files = doc["files"].to<JsonArray>();
    scanMp3("/", files, /*depth=*/2, /*maxFiles=*/64);
    String body; serializeJson(doc, body);
    _sendJson(200, body);
}

// ---------------------------------------------------------------------------
// Weather
// ---------------------------------------------------------------------------
static constexpr const char* SD_WEATHER_JSON = "/weather.json";

void WebUiServer::_apiWeatherGet() {
    JsonDocument doc;
    if (SD.exists(SD_WEATHER_JSON)) {
        File f = SD.open(SD_WEATHER_JSON, FILE_READ);
        if (f) { deserializeJson(doc, f); f.close(); }
    }
    // Always echo the canonical key set so the form has defaults.
    if (doc["WeatherAPIKey"].isNull()) doc["WeatherAPIKey"] = "";
    if (doc["lat"].isNull())           doc["lat"]   = 0.0f;
    if (doc["lon"].isNull())           doc["lon"]   = 0.0f;
    if (doc["units"].isNull())         doc["units"] = "metric";
    if (doc["lang"].isNull())          doc["lang"]  = "de";
    String body; serializeJson(doc, body);
    _sendJson(200, body);
}

void WebUiServer::_apiWeatherPut() {
    JsonDocument body;
    if (!_readJsonBody(body)) return;
    // Merge with existing file so unknown keys survive.
    JsonDocument doc;
    if (SD.exists(SD_WEATHER_JSON)) {
        File f = SD.open(SD_WEATHER_JSON, FILE_READ);
        if (f) { deserializeJson(doc, f); f.close(); }
    }
    for (JsonPair p : body.as<JsonObject>()) {
        doc[p.key()] = p.value();
    }
    File f = SD.open(SD_WEATHER_JSON, FILE_WRITE);
    if (!f) return _sendError(500, "cannot open /weather.json for writing");
    serializeJson(doc, f);
    f.close();
    // Re-read into WeatherManager and force an immediate refetch.
    _weather->begin();
    _weather->requestUpdate();
    _apiWeatherGet();
}

// ---------------------------------------------------------------------------
// Geocoding proxy (OpenWeatherMap Geocoding API)
// ---------------------------------------------------------------------------
static String urlEncodeStr(const String& s) {
    static const char hex[] = "0123456789ABCDEF";
    String out;
    out.reserve(s.length() * 3);
    for (size_t i = 0; i < s.length(); ++i) {
        unsigned char c = (unsigned char)s[i];
        if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') ||
            (c >= 'a' && c <= 'z') || c == '-' || c == '_' || c == '.' || c == '~') {
            out += (char)c;
        } else {
            out += '%';
            out += hex[(c >> 4) & 0xF];
            out += hex[c & 0xF];
        }
    }
    return out;
}

// ---------------------------------------------------------------------------
// SD card file manager
// ---------------------------------------------------------------------------
// Normalises `p` ("" -> "/", strips trailing '/') and rejects anything that
// could escape the card root or contains control characters.
static bool sdPathValid(String& p) {
    if (p.length() == 0) p = "/";
    if (p[0] != '/' || p.length() > 200) return false;
    if (p.indexOf("..") >= 0 || p.indexOf('\\') >= 0) return false;
    for (size_t i = 0; i < p.length(); ++i) {
        if ((unsigned char)p[i] < 0x20) return false;
    }
    while (p.length() > 1 && p.endsWith("/")) p.remove(p.length() - 1);
    return true;
}

static String sdJoin(const String& dir, const String& name) {
    return dir == "/" ? "/" + name : dir + "/" + name;
}

void WebUiServer::_apiSdList() {
    String path = _server.arg("path");
    if (!sdPathValid(path)) return _sendError(400, "invalid path");

    File dir = SD.open(path.c_str());
    if (!dir) return _sendError(404, "not found");
    if (!dir.isDirectory()) { dir.close(); return _sendError(400, "not a directory"); }

    static constexpr size_t MAX_ENTRIES = 500;
    JsonDocument doc;
    doc["path"]  = path;
    doc["total"] = (uint64_t)SD.totalBytes();
    doc["used"]  = (uint64_t)SD.usedBytes();
    JsonArray entries = doc["entries"].to<JsonArray>();

    size_t n = 0;
    File f = dir.openNextFile();
    while (f) {
        if (n >= MAX_ENTRIES) { doc["truncated"] = true; f.close(); break; }
        JsonObject e = entries.add<JsonObject>();
        e["name"] = String(f.name());
        e["dir"]  = f.isDirectory();
        e["size"] = f.isDirectory() ? 0u : (uint32_t)f.size();
        ++n;
        f.close();
        f = dir.openNextFile();
    }
    dir.close();

    String body; serializeJson(doc, body);
    _sendJson(200, body);
}

void WebUiServer::_apiSdDownload() {
    String path = _server.arg("path");
    if (!sdPathValid(path) || path == "/") return _sendError(400, "invalid path");

    File f = SD.open(path.c_str(), FILE_READ);
    if (!f) return _sendError(404, "not found");
    if (f.isDirectory()) { f.close(); return _sendError(400, "is a directory"); }

    const String name = path.substring(path.lastIndexOf('/') + 1);
    String ascii;
    for (size_t i = 0; i < name.length(); ++i) {
        const unsigned char c = (unsigned char)name[i];
        ascii += (c >= 0x20 && c < 0x7F && c != '"' && c != '\\') ? (char)c : '_';
    }
    _addCors();
    _server.sendHeader("Content-Disposition",
        "attachment; filename=\"" + ascii + "\"; filename*=UTF-8''" + urlEncodeStr(name));
    _server.streamFile(f, "application/octet-stream");
    f.close();
}

// Called by the WebServer for every chunk of the multipart body. The target
// directory comes from the ?path= query argument (parsed before the body).
void WebUiServer::_onSdUpload(HTTPUpload& up) {
    if (up.status == UPLOAD_FILE_START) {
        if (_uploadFile) _uploadFile.close();
        _uploadOk    = false;
        _uploadError = "";
        _uploadName  = "";
        _uploadBytes = 0;

        String dir = _server.arg("path");
        if (!sdPathValid(dir))   { _uploadError = "invalid path"; return; }

        String name = up.filename;
        const int slash = max(name.lastIndexOf('/'), name.lastIndexOf('\\'));
        if (slash >= 0) name = name.substring(slash + 1);
        if (name.length() == 0 || name.indexOf("..") >= 0) {
            _uploadError = "invalid file name"; return;
        }
        File d = SD.open(dir.c_str());
        const bool dirOk = d && d.isDirectory();
        if (d) d.close();
        if (!dirOk) { _uploadError = "target directory not found"; return; }

        _uploadName = sdJoin(dir, name);
        _uploadFile = SD.open(_uploadName.c_str(), FILE_WRITE);
        if (!_uploadFile) _uploadError = "cannot create file";
    } else if (up.status == UPLOAD_FILE_WRITE) {
        if (!_uploadFile) return;
        if (_uploadFile.write(up.buf, up.currentSize) != up.currentSize) {
            _uploadError = "write failed (SD card full?)";
            _uploadFile.close();
            SD.remove(_uploadName.c_str());
            return;
        }
        _uploadBytes += up.currentSize;
    } else if (up.status == UPLOAD_FILE_END) {
        if (_uploadFile) {
            _uploadFile.close();
            _uploadOk = _uploadError.length() == 0;
        }
    } else if (up.status == UPLOAD_FILE_ABORTED) {
        if (_uploadFile) {
            _uploadFile.close();
            SD.remove(_uploadName.c_str());
        }
        _uploadOk = false;
    }
}

void WebUiServer::_apiSdUploadDone() {
    if (!_uploadOk) {
        return _sendError(_uploadError.length() ? 400 : 500,
                          _uploadError.length() ? _uploadError.c_str() : "no file received");
    }
    JsonDocument doc;
    doc["ok"]   = true;
    doc["path"] = _uploadName;
    doc["size"] = _uploadBytes;
    String body; serializeJson(doc, body);
    _sendJson(200, body);
}

void WebUiServer::_apiSdDelete() {
    String path = _server.arg("path");
    if (!sdPathValid(path) || path == "/") return _sendError(400, "invalid path");
    if (!SD.exists(path.c_str())) return _sendError(404, "not found");

    File f = SD.open(path.c_str());
    const bool isDir = f && f.isDirectory();
    if (f) f.close();

    if (isDir ? SD.rmdir(path.c_str()) : SD.remove(path.c_str())) return _sendOk();
    _sendError(409, isDir ? "directory not empty" : "delete failed");
}

void WebUiServer::_apiSdMkdir() {
    String path = _server.arg("path");
    if (!sdPathValid(path) || path == "/") return _sendError(400, "invalid path");
    if (SD.exists(path.c_str())) return _sendError(409, "already exists");
    if (!SD.mkdir(path.c_str())) return _sendError(500, "mkdir failed");
    _sendOk();
}

// Renames within the same directory: ?path=/dir/old&name=new
void WebUiServer::_apiSdRename() {
    String path = _server.arg("path");
    String name = _server.arg("name");
    name.trim();
    if (!sdPathValid(path) || path == "/") return _sendError(400, "invalid path");
    if (name.length() == 0 || name.length() > 100 || name.indexOf('/') >= 0 ||
        name.indexOf('\\') >= 0 || name.indexOf("..") >= 0) {
        return _sendError(400, "invalid name");
    }
    if (!SD.exists(path.c_str())) return _sendError(404, "not found");

    String parent = path.substring(0, path.lastIndexOf('/'));
    if (parent.length() == 0) parent = "/";
    const String target = sdJoin(parent, name);
    if (target == path) return _sendOk();
    if (SD.exists(target.c_str())) return _sendError(409, "already exists");
    if (!SD.rename(path.c_str(), target.c_str())) return _sendError(500, "rename failed");
    _sendOk();
}

// Moves a file or folder into another directory: ?path=/a/x&to=/b
void WebUiServer::_apiSdMove() {
    String path = _server.arg("path");
    String to   = _server.arg("to");
    if (!sdPathValid(path) || path == "/" || !sdPathValid(to)) return _sendError(400, "invalid path");
    if (!SD.exists(path.c_str())) return _sendError(404, "not found");

    File d = SD.open(to.c_str());
    const bool dirOk = d && d.isDirectory();
    if (d) d.close();
    if (!dirOk) return _sendError(400, "target is not a directory");
    if (to == path || to.startsWith(path + "/")) return _sendError(400, "cannot move into itself");

    const String target = sdJoin(to, path.substring(path.lastIndexOf('/') + 1));
    if (target == path) return _sendOk();
    if (SD.exists(target.c_str())) return _sendError(409, "already exists");
    if (!SD.rename(path.c_str(), target.c_str())) return _sendError(500, "move failed");
    _sendOk();
}

// Plays an MP3 on the device itself (the player only supports MP3). Streaming
// audio to the browser would block the single-threaded web server.
void WebUiServer::_apiSdPlay() {
    String path = _server.arg("path");
    if (!sdPathValid(path) || path == "/") return _sendError(400, "invalid path");
    String lower = path; lower.toLowerCase();
    if (!lower.endsWith(".mp3")) return _sendError(400, "only MP3 files can be played");

    File f = SD.open(path.c_str(), FILE_READ);
    const bool ok = f && !f.isDirectory();
    if (f) f.close();
    if (!ok) return _sendError(404, "not found");

    _audio->playFile(path.c_str(), /*loop=*/false);
    _sendOk();
}

void WebUiServer::_apiSdStop() {
    _audio->stop();
    _sendOk();
}

static String readOwmApiKey() {
    JsonDocument doc;
    if (SD.exists(SD_WEATHER_JSON)) {
        File f = SD.open(SD_WEATHER_JSON, FILE_READ);
        if (f) { deserializeJson(doc, f); f.close(); }
    }
    const char* k = doc["WeatherAPIKey"] | "";
    return String(k);
}

void WebUiServer::_geocodeProxy(const String& path) {
    const String key = readOwmApiKey();
    if (key.length() == 0) return _sendError(400, "WeatherAPIKey not set");

    String url = "http://api.openweathermap.org/geo/1.0/";
    url += path;
    url += (path.indexOf('?') >= 0 ? '&' : '?');
    url += "appid=";
    url += key;

    HTTPClient http;
    http.setConnectTimeout(5000);
    http.setTimeout(8000);
    http.setReuse(false);
    if (!http.begin(url)) return _sendError(502, "http.begin failed");

    const int code = http.GET();
    if (code <= 0) {
        const String msg = http.errorToString(code);
        http.end();
        return _sendError(502, msg.c_str());
    }
    String body = http.getString();
    http.end();
    _addCors();
    _server.send(code, "application/json; charset=utf-8", body);
}

void WebUiServer::_apiGeocode() {
    if (!_server.hasArg("q")) return _sendError(400, "missing q");
    const int limitArg = _server.hasArg("limit") ? _server.arg("limit").toInt() : 5;
    const int limit = (limitArg < 1) ? 1 : (limitArg > 10 ? 10 : limitArg);
    String path = "direct?q=" + urlEncodeStr(_server.arg("q"))
                + "&limit=" + String(limit);
    _geocodeProxy(path);
}

void WebUiServer::_apiGeocodeReverse() {
    if (!_server.hasArg("lat") || !_server.hasArg("lon"))
        return _sendError(400, "missing lat/lon");
    const int limitArg = _server.hasArg("limit") ? _server.arg("limit").toInt() : 1;
    const int limit = (limitArg < 1) ? 1 : (limitArg > 5 ? 5 : limitArg);
    String path = "reverse?lat=" + _server.arg("lat")
                + "&lon=" + _server.arg("lon")
                + "&limit=" + String(limit);
    _geocodeProxy(path);
}
