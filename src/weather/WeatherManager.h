#pragma once
#include <Arduino.h>
#include <stdint.h>
#include <time.h>
#include <vector>

/**
 * WeatherManager
 *
 * Periodically fetches the current and forecast weather from
 * OpenWeatherMap's One Call 3.0 API and exposes it to the UI.
 *
 * Configuration is read once at begin() from /weather.json on the SD
 * card:
 *   {
 *     "WeatherAPIKey": "...",
 *     "lat": 50.1109,
 *     "lon": 8.6821,
 *     "units": "metric",
 *     "lang":  "de"
 *   }
 *
 * Five logical "slots" are produced:
 *   - current()    — current weather (temp, feels_like, description)
 *   - morning()    — today.morn        (~ 06:00–11:00)
 *   - afternoon()  — today.day         (~ 12:00–17:00)
 *   - evening()    — today.eve         (~ 18:00–22:00)
 *   - tomorrow()   — daily[1].day      (next-day midday)
 *
 * Use loop() in the Arduino main loop; it returns true exactly when a
 * fresh poll has succeeded so the caller can refresh the UI.
 */
class WeatherManager {
public:
    struct Slot {
        float    temp        = 0.0f;     // °C (units=metric)
        float    feels        = 0.0f;     // °C
        int      pop         = -1;       // 0..100 % rain probability, -1 = N/A
        char     icon[8]     = {0};      // OpenWeatherMap icon code, e.g. "01d"
        char     desc[48]    = {0};      // localized description (current only)
        bool     valid       = false;
    };

    /**
     * One entry of the next-24-hours forecast used by the Weather settings
     * panel. Filled in chronological order, oldest (= the current hour)
     * first. Empty when hasData() is false or before the first fetch.
     */
    struct HourPoint {
        time_t   ts       = 0;       // unix epoch (UTC) of the hour
        float    temp     = 0.0f;    // °C
        int      rainPct  = 0;       // 0..100 % — pop% when condition is rain/mix
        int      snowPct  = 0;       // 0..100 % — pop% when weather id is 600..622
        char     icon[8]  = {0};     // OWM icon code
    };

    /** Load /weather.json from SD. Returns false if the file is missing
     *  or the API key is still the "MyWeatherAPIKey" placeholder. */
    bool begin();

    /** Drive periodic polling. Returns true exactly once per successful
     *  fetch so the caller can refresh dependent UI. */
    bool loop();

    /** Force the next loop() iteration to attempt a fetch immediately. */
    void requestUpdate() { _lastAttemptMs = 0; }

    bool        isConfigured() const { return _configured; }
    bool        hasData()      const { return _hasData; }

    const Slot& current()   const { return _current; }
    const Slot& morning()   const { return _morn; }
    const Slot& afternoon() const { return _aft; }
    const Slot& evening()   const { return _eve; }
    const Slot& tomorrow()  const { return _tom; }

    // Today's overall min/max (daily[0].temp.min/max). Valid only when
    // hasData() is true; otherwise 0.0f.
    float       todayMin()  const { return _todayMin; }
    float       todayMax()  const { return _todayMax; }

    /** Next-24-hours forecast (0..24 entries, chronological from now). */
    const std::vector<HourPoint>& hourly() const { return _hourly; }

    /** Monotonic counter incremented after every successful fetch parse.
     *  UI panels can compare against a cached value to skip work. */
    uint32_t    version()    const { return _version; }

private:
    bool _fetch();

    String   _key;
    float    _lat            = 0.0f;
    float    _lon            = 0.0f;
    String   _units          = "metric";
    String   _lang           = "de";

    bool     _configured     = false;
    bool     _hasData        = false;
    uint32_t _lastAttemptMs  = 0;     // millis() of last fetch attempt
    uint32_t _retryDelayMs   = 60000; // backoff after failures

    Slot     _current;
    Slot     _morn;
    Slot     _aft;
    Slot     _eve;
    Slot     _tom;
    float    _todayMin = 0.0f;
    float    _todayMax = 0.0f;

    std::vector<HourPoint> _hourly;
    uint32_t _version  = 0;

    // Poll once every five minutes when WiFi is up.
    static constexpr uint32_t POLL_INTERVAL_MS = 5UL * 60UL * 1000UL;
};
