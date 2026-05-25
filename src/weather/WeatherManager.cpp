#include "WeatherManager.h"

#include <SD.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

#include "../serial_safe.h"

// ---------------------------------------------------------------------------
// SD config path
// ---------------------------------------------------------------------------
static constexpr const char* SD_WEATHER_JSON = "/weather.json";

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
static void copyStr(char* dst, size_t dstLen, const char* src) {
    if (!src) { dst[0] = '\0'; return; }
    strncpy(dst, src, dstLen - 1);
    dst[dstLen - 1] = '\0';
}

// ---------------------------------------------------------------------------
// begin()
// ---------------------------------------------------------------------------
bool WeatherManager::begin() {
    _configured = false;

    if (!SD.exists(SD_WEATHER_JSON)) {
        serial_safe_println("[Weather] /weather.json missing on SD");
        return false;
    }

    File f = SD.open(SD_WEATHER_JSON, FILE_READ);
    if (!f) {
        serial_safe_println("[Weather] cannot open /weather.json");
        return false;
    }

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, f);
    f.close();
    if (err) {
        serial_safe_printf("[Weather] /weather.json parse error: %s\n", err.c_str());
        return false;
    }

    _key   = doc["WeatherAPIKey"] | "";
    _lat   = doc["lat"]   | 0.0f;
    _lon   = doc["lon"]   | 0.0f;
    _units = doc["units"] | "metric";
    _lang  = doc["lang"]  | "de";

    if (_key.length() == 0 || _key == "MyWeatherAPIKey") {
        serial_safe_println("[Weather] WeatherAPIKey not configured in /weather.json");
        return false;
    }
    if (_lat == 0.0f && _lon == 0.0f) {
        serial_safe_println("[Weather] lat/lon not set in /weather.json");
        return false;
    }

    _configured = true;
    serial_safe_printf("[Weather] configured: lat=%.4f lon=%.4f units=%s lang=%s\n",
                       _lat, _lon, _units.c_str(), _lang.c_str());
    return true;
}

// ---------------------------------------------------------------------------
// loop()
// ---------------------------------------------------------------------------
bool WeatherManager::loop() {
    if (!_configured) return false;
    if (WiFi.status() != WL_CONNECTED) return false;

    const uint32_t now = millis();
    const uint32_t interval = _hasData ? POLL_INTERVAL_MS : _retryDelayMs;

    if (_lastAttemptMs != 0 && (now - _lastAttemptMs) < interval) return false;

    _lastAttemptMs = now;
    return _fetch();
}

// ---------------------------------------------------------------------------
// _fetch()
// ---------------------------------------------------------------------------
bool WeatherManager::_fetch() {
    char url[256];
    snprintf(url, sizeof(url),
             "http://api.openweathermap.org/data/3.0/onecall"
             "?lat=%.4f&lon=%.4f&exclude=minutely,alerts"
             "&units=%s&lang=%s&appid=%s",
             _lat, _lon, _units.c_str(), _lang.c_str(), _key.c_str());

    HTTPClient http;
    http.setConnectTimeout(5000);
    http.setTimeout(8000);
    http.setReuse(false);

    if (!http.begin(url)) {
        serial_safe_println("[Weather] http.begin() failed");
        return false;
    }

    const int code = http.GET();
    if (code != 200) {
        serial_safe_printf("[Weather] HTTP %d (%s)\n", code,
                           http.errorToString(code).c_str());
        http.end();
        return false;
    }

    // Filter the response down to the few fields we actually use so the
    // JSON document stays small.  Without the filter the daily array
    // alone is ~6 KB.
    JsonDocument filter;
    JsonObject f_current = filter["current"].to<JsonObject>();
    f_current["temp"]       = true;
    f_current["feels_like"] = true;
    JsonObject f_curWeather = f_current["weather"][0].to<JsonObject>();
    f_curWeather["icon"]        = true;
    f_curWeather["description"] = true;

    JsonObject f_daily = filter["daily"][0].to<JsonObject>();
    f_daily["pop"]              = true;
    JsonObject f_temp           = f_daily["temp"].to<JsonObject>();
    f_temp["morn"]              = true;
    f_temp["day"]               = true;
    f_temp["eve"]               = true;
    f_temp["min"]               = true;
    f_temp["max"]               = true;
    JsonObject f_feels          = f_daily["feels_like"].to<JsonObject>();
    f_feels["morn"]             = true;
    f_feels["day"]              = true;
    f_feels["eve"]              = true;
    JsonObject f_dWeather       = f_daily["weather"][0].to<JsonObject>();
    f_dWeather["icon"]          = true;

    // hourly[*] — index 0 in the filter applies to every array element.
    JsonObject f_hourly         = filter["hourly"][0].to<JsonObject>();
    f_hourly["dt"]              = true;
    f_hourly["temp"]            = true;
    f_hourly["pop"]             = true;
    JsonObject f_hWeather       = f_hourly["weather"][0].to<JsonObject>();
    f_hWeather["id"]            = true;
    f_hWeather["icon"]          = true;

    JsonDocument doc;
    DeserializationError err = deserializeJson(
        doc, http.getStream(),
        DeserializationOption::Filter(filter));
    http.end();

    if (err) {
        serial_safe_printf("[Weather] JSON parse error: %s\n", err.c_str());
        return false;
    }

    JsonVariant cur = doc["current"];
    if (cur.isNull()) {
        serial_safe_println("[Weather] missing 'current' in response");
        return false;
    }

    // Current ----------------------------------------------------------------
    _current.temp  = cur["temp"]       | 0.0f;
    _current.feels = cur["feels_like"] | 0.0f;
    _current.pop   = -1;
    copyStr(_current.icon, sizeof(_current.icon),
            cur["weather"][0]["icon"] | "");
    copyStr(_current.desc, sizeof(_current.desc),
            cur["weather"][0]["description"] | "");
    _current.valid = (_current.icon[0] != '\0');

    // Today (daily[0]) -------------------------------------------------------
    JsonVariant d0 = doc["daily"][0];
    JsonVariant d1 = doc["daily"][1];

    auto fillForecast = [](Slot& s, JsonVariant d, const char* tempKey) {
        if (d.isNull()) { s.valid = false; return; }
        s.temp  = d["temp"][tempKey]       | 0.0f;
        s.feels = d["feels_like"][tempKey] | 0.0f;
        const float pop = d["pop"] | 0.0f;
        s.pop   = (int)lroundf(pop * 100.0f);
        copyStr(s.icon, sizeof(s.icon),
                d["weather"][0]["icon"] | "");
        s.desc[0] = '\0';
        s.valid   = (s.icon[0] != '\0');
    };

    fillForecast(_morn, d0, "morn");
    fillForecast(_aft,  d0, "day");
    fillForecast(_eve,  d0, "eve");
    fillForecast(_tom,  d1, "day");

    if (!d0.isNull()) {
        _todayMin = d0["temp"]["min"] | 0.0f;
        _todayMax = d0["temp"]["max"] | 0.0f;
    }

    // Hourly forecast (next 24 entries, dropping past hours when the
    // system clock is already NTP-synced). OWM weather id 600..622 marks
    // snow-class conditions; we split the single `pop` field into a
    // rain/snow pair using that bucket. OpenWeather does not expose a
    // dedicated snow probability per hour.
    _hourly.clear();
    JsonArray ha = doc["hourly"].as<JsonArray>();
    if (!ha.isNull()) {
        _hourly.reserve(24);
        const time_t nowTs = time(nullptr);
        const bool   timeOk = (nowTs > 1700000000);   // any plausible post-2023 epoch
        for (JsonObject h : ha) {
            if (_hourly.size() >= 24) break;
            time_t dt = h["dt"] | (time_t)0;
            if (timeOk && dt + 1800 < nowTs) continue;   // skip hours >30 min in the past
            HourPoint p;
            p.ts   = dt;
            p.temp = h["temp"] | 0.0f;
            const float pop    = h["pop"] | 0.0f;
            const int   popPct = (int)lroundf(pop * 100.0f);
            const int   wid    = h["weather"][0]["id"] | 0;
            const bool  isSnow = (wid >= 600 && wid <= 622);
            p.rainPct = isSnow ? 0      : popPct;
            p.snowPct = isSnow ? popPct : 0;
            copyStr(p.icon, sizeof(p.icon), h["weather"][0]["icon"] | "");
            _hourly.push_back(p);
        }
    }

    _hasData = _current.valid;
    ++_version;
    serial_safe_printf("[Weather] update OK: cur=%.1f°C feels=%.1f icon=%s '%s' "
                       "morn=%.1f/%d%% aft=%.1f/%d%% eve=%.1f/%d%% tom=%.1f/%d%%\n",
                       _current.temp, _current.feels, _current.icon, _current.desc,
                       _morn.temp, _morn.pop,
                       _aft.temp,  _aft.pop,
                       _eve.temp,  _eve.pop,
                       _tom.temp,  _tom.pop);
    return _hasData;
}
