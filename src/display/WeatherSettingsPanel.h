#pragma once
#include <Arduino.h>
#include <lvgl.h>

class WeatherManager;

/**
 * WeatherSettingsPanel — "Wetter" tab inside SettingsScreen's tabview.
 *
 * Layout (top → bottom):
 *   - Hero row: current weather icon + temperature + description + today's
 *     min/max. Same data the AlarmScreen hero card shows.
 *   - Forecast tile row: Vormittag / Nachmittag / Abend / Morgen — mirrors
 *     AlarmScreen forecast tiles in a compact form.
 *   - Hourly chart card: horizontally scrollable lv_chart with three
 *     series (temperature, rain %, snow %) for the next 24 hours.
 *
 * tick() is cheap when nothing changed — it skips work unless
 * WeatherManager::version() has advanced since the last repaint.
 */
class WeatherSettingsPanel {
public:
    /** Build the panel UI inside parent (a tab content container). */
    void create(lv_obj_t* parent);

    /** Force a repaint on the next tick() — call after the panel becomes
     *  visible so the data refreshes immediately. */
    void requestRefresh() { _lastVersion = 0xFFFFFFFFu; }

    /** Refresh widgets from `w` when its version() has advanced.
     *  Safe to call every second; no-op when invalidate()d. */
    void tick(const WeatherManager& w);

    /** Null all widget pointers — call after the host screen is destroyed
     *  to prevent dangling-pointer use on the next settings-open. */
    void invalidate();

    // Public so file-scope helpers in the .cpp can manipulate the
    // forecast tiles without needing to be class members.
    struct Tile {
        lv_obj_t* card = nullptr;
        lv_obj_t* head = nullptr;
        lv_obj_t* temp = nullptr;
        lv_obj_t* pop  = nullptr;
        lv_obj_t* icon = nullptr;
        char      iconCode[8] = {0};
    };

private:
    lv_obj_t* _root          = nullptr;

    // Hero
    lv_obj_t* _imgHeroIcon   = nullptr;
    lv_obj_t* _lblHeroTemp   = nullptr;
    lv_obj_t* _lblHeroUnit   = nullptr;
    lv_obj_t* _lblHeroDesc   = nullptr;
    lv_obj_t* _lblHeroMinMax = nullptr;
    char      _heroIconCode[8] = {0};

    // Forecast tiles
    Tile _tMorn;
    Tile _tAft;
    Tile _tEve;
    Tile _tTom;

    // Hourly chart
    lv_obj_t*       _chartScroll = nullptr;   // horizontally-scrollable container
    lv_obj_t*       _chart       = nullptr;
    lv_chart_series_t* _serTemp  = nullptr;
    lv_chart_series_t* _serRain  = nullptr;
    lv_chart_series_t* _serSnow  = nullptr;
    lv_obj_t*       _hourLabels[9] = {nullptr};   // X-axis hour labels (every 3 h)
    lv_obj_t*       _lblChartMsg = nullptr;       // shown when no data yet

    uint32_t _lastVersion = 0;
};

extern WeatherSettingsPanel weatherSettingsPanel;
