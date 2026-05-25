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
    using Fn = std::function<void()>;
    explicit WildcardHandler(Fn fn) : _fn(std::move(fn)) {}
    bool canHandle(HTTPMethod, String) override { return true; }
    bool handle(WebServer&, HTTPMethod, String) override { _fn(); return true; }
private:
    Fn _fn;
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
    _server.addHandler(new WildcardHandler([this]() { _handle(); }));

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
