// =====================================================================
//  WeatherSettingsPanel.cpp  —  "Wetter" tab of SettingsScreen
// =====================================================================
#include "WeatherSettingsPanel.h"
#include "AlarmScreen.h"               // weatherIconCacheLoad()
#include "weather/WeatherManager.h"
#include "../serial_safe.h"

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <time.h>

WeatherSettingsPanel weatherSettingsPanel;

// External fonts (declared elsewhere as extern "C")
extern "C" const lv_font_t ui_font_ms14m;
extern "C" const lv_font_t ui_font_ms24m;
extern "C" const lv_font_t ui_font_ms36m;
extern "C" const lv_font_t ui_font_ms80m;

namespace {

// Palette — matches the light/blue System tab.
constexpr uint32_t BG          = 0xF1F5F9;
constexpr uint32_t CARD_BG     = 0xFFFFFF;
constexpr uint32_t CARD_BORDER = 0xCBD5E1;
constexpr uint32_t TITLE       = 0x1E293B;
constexpr uint32_t SUB         = 0x64748B;
constexpr uint32_t TEMP_COL    = 0x2563EB;     // primary accent — temperature
constexpr uint32_t RAIN_COL    = 0x0EA5E9;     // sky blue — rain
constexpr uint32_t SNOW_COL    = 0x94A3B8;     // slate — snow
constexpr uint32_t CHART_GRID  = 0xE2E8F0;

// Panel geometry (the tab content area is ~800 × 430).
constexpr int PANEL_W       = 800;
constexpr int HERO_Y        = 8;
constexpr int HERO_H        = 110;
constexpr int TILES_Y       = HERO_Y + HERO_H + 8;       // 126
constexpr int TILES_H       = 108;
constexpr int CHART_CARD_Y  = TILES_Y + TILES_H + 8;     // 242
constexpr int CHART_CARD_H  = 178;

// Hourly chart geometry.
constexpr int  HOURS              = 24;
constexpr int  CHART_INNER_W      = 1200;     // wider than panel → scrolls
constexpr int  CHART_INNER_H      = 110;
constexpr int  CHART_X_MARGIN     = 24;       // left/right inset inside chart
constexpr int  HOUR_LABEL_STEP    = 3;        // label every N hours (9 labels)
constexpr int  HOUR_LABEL_COUNT   = (HOURS / HOUR_LABEL_STEP) + 1;  // 9

// Forecast tile geometry — 4 tiles, 188×108 each, 12 + 4*188 + 3*8 + 12 = 800.
constexpr int TILE_W = 188;
constexpr int TILE_H = TILES_H;

// ---------------------------------------------------------------------------
// Small widget helpers
// ---------------------------------------------------------------------------
lv_obj_t* makeLabel(lv_obj_t* parent, const char* txt, const lv_font_t* font,
                    uint32_t color, int x, int y) {
    lv_obj_t* l = lv_label_create(parent);
    lv_label_set_text(l, txt);
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, lv_color_hex(color), 0);
    lv_obj_set_pos(l, x, y);
    return l;
}

lv_obj_t* makeCard(lv_obj_t* parent, int x, int y, int w, int h) {
    lv_obj_t* c = lv_obj_create(parent);
    lv_obj_set_pos(c, x, y);
    lv_obj_set_size(c, w, h);
    lv_obj_set_style_bg_color(c, lv_color_hex(CARD_BG), 0);
    lv_obj_set_style_bg_opa(c, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(c, lv_color_hex(CARD_BORDER), 0);
    lv_obj_set_style_border_width(c, 1, 0);
    lv_obj_set_style_radius(c, 8, 0);
    lv_obj_set_style_pad_all(c, 6, 0);
    lv_obj_clear_flag(c, LV_OBJ_FLAG_SCROLLABLE);
    return c;
}

void setSlotText(lv_obj_t* tempLbl, lv_obj_t* popLbl,
                 const WeatherManager::Slot& s) {
    char buf[40];
    if (s.valid) snprintf(buf, sizeof(buf), "%.0f\xc2\xb0""C", (double)s.temp);
    else         snprintf(buf, sizeof(buf), "--");
    lv_label_set_text(tempLbl, buf);
    if (popLbl) {
        if (s.valid && s.pop >= 0)
            snprintf(buf, sizeof(buf), LV_SYMBOL_TINT " %d %%", s.pop);
        else
            snprintf(buf, sizeof(buf), LV_SYMBOL_TINT " --");
        lv_label_set_text(popLbl, buf);
    }
}

void applyTileIcon(WeatherSettingsPanel::Tile& t,
                   const WeatherManager::Slot& s) {
    if (!t.icon || !s.valid) return;
    if (strncmp(t.iconCode, s.icon, sizeof(t.iconCode)) == 0) return;
    const lv_image_dsc_t* dsc = weatherIconCacheLoad(s.icon);
    if (!dsc) return;
    lv_image_set_src(t.icon, dsc);
    lv_obj_clear_flag(t.icon, LV_OBJ_FLAG_HIDDEN);
    strncpy(t.iconCode, s.icon, sizeof(t.iconCode) - 1);
    t.iconCode[sizeof(t.iconCode) - 1] = '\0';
}

// Legend chip = small filled rectangle + label.
void makeLegendChip(lv_obj_t* parent, int x, int y, uint32_t color,
                    const char* text) {
    lv_obj_t* dot = lv_obj_create(parent);
    lv_obj_set_size(dot, 14, 14);
    lv_obj_set_pos(dot, x, y + 2);
    lv_obj_set_style_bg_color(dot, lv_color_hex(color), 0);
    lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(dot, 0, 0);
    lv_obj_set_style_radius(dot, 3, 0);
    lv_obj_set_style_pad_all(dot, 0, 0);
    lv_obj_clear_flag(dot, LV_OBJ_FLAG_SCROLLABLE);
    makeLabel(parent, text, &ui_font_ms14m, TITLE, x + 22, y);
}

} // namespace

// ---------------------------------------------------------------------------
// invalidate / create
// ---------------------------------------------------------------------------
void WeatherSettingsPanel::invalidate() {
    _root          = nullptr;
    _imgHeroIcon   = _lblHeroTemp = _lblHeroUnit = nullptr;
    _lblHeroDesc   = _lblHeroMinMax = nullptr;
    _heroIconCode[0] = '\0';
    _tMorn = _tAft = _tEve = _tTom = Tile{};
    _chartScroll = _chart = nullptr;
    _serTemp = _serRain = _serSnow = nullptr;
    for (int i = 0; i < HOUR_LABEL_COUNT; ++i) _hourLabels[i] = nullptr;
    _lblChartMsg = nullptr;
    _lastVersion = 0;
}

void WeatherSettingsPanel::create(lv_obj_t* parent) {
    _root = parent;
    lv_obj_clean(parent);
    lv_obj_set_style_bg_color(parent, lv_color_hex(BG), 0);
    lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(parent, 0, 0);
    lv_obj_set_style_border_width(parent, 0, 0);
    lv_obj_clear_flag(parent, LV_OBJ_FLAG_SCROLLABLE);

    // ---------------- Hero "Jetzt" card ----------------
    lv_obj_t* hero = makeCard(parent, 12, HERO_Y, PANEL_W - 24, HERO_H);

    // Icon (left). Scale 50×50 OWM PNG to fit ~80×80 (256 = 1.0).
    _imgHeroIcon = lv_image_create(hero);
    lv_image_set_scale(_imgHeroIcon, 410);     // 50 * 410/256 ≈ 80 px
    lv_image_set_inner_align(_imgHeroIcon, LV_IMAGE_ALIGN_CENTER);
    lv_obj_set_size(_imgHeroIcon, 90, 90);
    lv_obj_set_pos(_imgHeroIcon, 0, 2);
    lv_obj_add_flag(_imgHeroIcon, LV_OBJ_FLAG_HIDDEN);

    makeLabel(hero, "Jetzt", &ui_font_ms14m, SUB, 100, 2);
    // ms80m is ASCII-only; °C suffix uses ms24m which supports 0x20–0xFF.
    _lblHeroTemp = makeLabel(hero, "--", &ui_font_ms80m, TEMP_COL, 100, 14);
    _lblHeroUnit = makeLabel(hero, "\xc2\xb0""C", &ui_font_ms24m, TEMP_COL, 230, 30);
    _lblHeroDesc = makeLabel(hero, "", &ui_font_ms24m, TITLE, 300, 18);
    lv_obj_set_width(_lblHeroDesc, PANEL_W - 24 - 300 - 12);
    lv_label_set_long_mode(_lblHeroDesc, LV_LABEL_LONG_DOT);
    _lblHeroMinMax = makeLabel(hero, "", &ui_font_ms24m, SUB, 300, 56);

    // ---------------- Forecast tiles ----------------
    auto makeTile = [&](const char* head, int x) -> Tile {
        lv_obj_t* card = makeCard(parent, x, TILES_Y, TILE_W, TILE_H);
        Tile t;
        t.card = card;
        t.head = makeLabel(card, head, &ui_font_ms24m, TITLE, 2, 0);
        t.temp = makeLabel(card, "--", &ui_font_ms36m, TEMP_COL, 2, 30);
        t.pop  = makeLabel(card, LV_SYMBOL_TINT " --", &lv_font_montserrat_14, SUB, 2, 74);
        t.icon = lv_image_create(card);
        lv_image_set_scale(t.icon, 256);
        lv_obj_set_size(t.icon, 70, 70);
        lv_obj_set_pos(t.icon, TILE_W - 90, 18);
        lv_obj_add_flag(t.icon, LV_OBJ_FLAG_HIDDEN);
        return t;
    };
    _tMorn = makeTile("Vormittag",  12);
    _tAft  = makeTile("Nachmittag", 208);
    _tEve  = makeTile("Abend",      404);
    _tTom  = makeTile("Morgen",     600);

    // ---------------- Hourly chart card ----------------
    lv_obj_t* chartCard = makeCard(parent, 12, CHART_CARD_Y,
                                   PANEL_W - 24, CHART_CARD_H);
    lv_obj_set_style_pad_all(chartCard, 6, 0);

    makeLabel(chartCard, "N\xc3\xa4""chste 24 Stunden",
              &ui_font_ms14m, TITLE, 4, 0);

    // Legend (top-right of card)
    makeLegendChip(chartCard, 220, 0,  TEMP_COL, "Temp \xc2\xb0""C");
    makeLegendChip(chartCard, 360, 0,  RAIN_COL, "Regen %");
    makeLegendChip(chartCard, 500, 0,  SNOW_COL, "Schnee %");

    // Horizontally-scrollable inner container — chart is wider than the card.
    _chartScroll = lv_obj_create(chartCard);
    lv_obj_set_pos(_chartScroll, 0, 22);
    lv_obj_set_size(_chartScroll, PANEL_W - 24 - 12, CHART_CARD_H - 32);
    lv_obj_set_style_bg_opa(_chartScroll, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(_chartScroll, 0, 0);
    lv_obj_set_style_pad_all(_chartScroll, 0, 0);
    lv_obj_set_scroll_dir(_chartScroll, LV_DIR_HOR);
    lv_obj_set_scrollbar_mode(_chartScroll, LV_SCROLLBAR_MODE_AUTO);

    _chart = lv_chart_create(_chartScroll);
    lv_obj_set_pos(_chart, 0, 0);
    lv_obj_set_size(_chart, CHART_INNER_W, CHART_INNER_H);
    lv_chart_set_type(_chart, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(_chart, HOURS);
    lv_chart_set_update_mode(_chart, LV_CHART_UPDATE_MODE_SHIFT);
    lv_chart_set_div_line_count(_chart, 5, 0);
    lv_obj_set_style_pad_all(_chart, 4, 0);
    lv_obj_set_style_pad_left(_chart, CHART_X_MARGIN, 0);
    lv_obj_set_style_pad_right(_chart, CHART_X_MARGIN, 0);
    lv_obj_set_style_bg_color(_chart, lv_color_hex(CARD_BG), 0);
    lv_obj_set_style_border_color(_chart, lv_color_hex(CARD_BORDER), 0);
    lv_obj_set_style_border_width(_chart, 1, 0);
    lv_obj_set_style_radius(_chart, 4, 0);
    lv_obj_set_style_line_color(_chart, lv_color_hex(CHART_GRID), LV_PART_MAIN);
    // Hide the per-point dots; they clutter the line at 24 px spacing.
    lv_obj_set_style_size(_chart, 0, 0, LV_PART_INDICATOR);

    _serTemp = lv_chart_add_series(_chart, lv_color_hex(TEMP_COL),
                                   LV_CHART_AXIS_PRIMARY_Y);
    _serRain = lv_chart_add_series(_chart, lv_color_hex(RAIN_COL),
                                   LV_CHART_AXIS_SECONDARY_Y);
    _serSnow = lv_chart_add_series(_chart, lv_color_hex(SNOW_COL),
                                   LV_CHART_AXIS_SECONDARY_Y);

    // Default Y ranges; refreshed from data in tick().
    lv_chart_set_range(_chart, LV_CHART_AXIS_PRIMARY_Y,   0,   30);
    lv_chart_set_range(_chart, LV_CHART_AXIS_SECONDARY_Y, 0,  100);

    // Hour labels (every 3 h: positions 0, 3, …, 24). Placed under the
    // chart inside the scroll container so they scroll together with it.
    for (int i = 0; i < HOUR_LABEL_COUNT; ++i) {
        _hourLabels[i] = makeLabel(_chartScroll, "--:--",
                                   &ui_font_ms14m, SUB, 0, CHART_INNER_H + 2);
    }

    // "No data yet" overlay — shown until the first successful fetch.
    _lblChartMsg = makeLabel(_chartScroll, "Noch keine Wetterdaten",
                             &ui_font_ms14m, SUB,
                             CHART_INNER_W / 2 - 80, CHART_INNER_H / 2 - 8);

    _lastVersion = 0;
}

// ---------------------------------------------------------------------------
// tick()  — called once per second by main loop while panel is visible
// ---------------------------------------------------------------------------
void WeatherSettingsPanel::tick(const WeatherManager& w) {
    if (!_root || !_chart) return;

    const uint32_t v = w.version();
    if (v == _lastVersion) return;
    _lastVersion = v;

    // ---- Hero ----
    const auto& cur = w.current();
    char buf[64];
    if (cur.valid) {
        snprintf(buf, sizeof(buf), "%.0f", (double)cur.temp);
        lv_label_set_text(_lblHeroTemp, buf);
        // Reposition the °C suffix flush against the number.
        lv_obj_update_layout(_lblHeroTemp);
        const int tempW = lv_obj_get_width(_lblHeroTemp);
        lv_obj_set_pos(_lblHeroUnit, 100 + tempW + 4, 30);

        lv_label_set_text(_lblHeroDesc, cur.desc);

        snprintf(buf, sizeof(buf), "Min %.0f\xc2\xb0 / Max %.0f\xc2\xb0",
                 (double)w.todayMin(), (double)w.todayMax());
        lv_label_set_text(_lblHeroMinMax, buf);

        if (strncmp(_heroIconCode, cur.icon, sizeof(_heroIconCode)) != 0) {
            const lv_image_dsc_t* dsc = weatherIconCacheLoad(cur.icon);
            if (dsc) {
                lv_image_set_src(_imgHeroIcon, dsc);
                lv_obj_clear_flag(_imgHeroIcon, LV_OBJ_FLAG_HIDDEN);
                strncpy(_heroIconCode, cur.icon, sizeof(_heroIconCode) - 1);
                _heroIconCode[sizeof(_heroIconCode) - 1] = '\0';
            }
        }
    } else {
        lv_label_set_text(_lblHeroTemp,   "--");
        lv_label_set_text(_lblHeroDesc,   "Keine Daten");
        lv_label_set_text(_lblHeroMinMax, "");
    }

    // ---- Forecast tiles ----
    setSlotText(_tMorn.temp, _tMorn.pop, w.morning());
    setSlotText(_tAft.temp,  _tAft.pop,  w.afternoon());
    setSlotText(_tEve.temp,  _tEve.pop,  w.evening());
    setSlotText(_tTom.temp,  _tTom.pop,  w.tomorrow());
    applyTileIcon(_tMorn, w.morning());
    applyTileIcon(_tAft,  w.afternoon());
    applyTileIcon(_tEve,  w.evening());
    applyTileIcon(_tTom,  w.tomorrow());

    // ---- Hourly chart ----
    const auto& hours = w.hourly();
    const size_t n    = hours.size();

    if (n == 0) {
        // No data → leave chart blank and show the message overlay.
        if (_lblChartMsg) lv_obj_clear_flag(_lblChartMsg, LV_OBJ_FLAG_HIDDEN);
        for (int i = 0; i < HOURS; ++i) {
            lv_chart_set_value_by_id(_chart, _serTemp, i, LV_CHART_POINT_NONE);
            lv_chart_set_value_by_id(_chart, _serRain, i, LV_CHART_POINT_NONE);
            lv_chart_set_value_by_id(_chart, _serSnow, i, LV_CHART_POINT_NONE);
        }
        lv_chart_refresh(_chart);
        return;
    }

    if (_lblChartMsg) lv_obj_add_flag(_lblChartMsg, LV_OBJ_FLAG_HIDDEN);

    // Compute temperature range (rounded outward to whole °C with 2° pad).
    float tMin =  1e9f, tMax = -1e9f;
    for (size_t i = 0; i < n; ++i) {
        if (hours[i].temp < tMin) tMin = hours[i].temp;
        if (hours[i].temp > tMax) tMax = hours[i].temp;
    }
    int yMin = (int)floorf(tMin) - 2;
    int yMax = (int)ceilf (tMax) + 2;
    if (yMax - yMin < 6) { yMax = yMin + 6; }      // avoid degenerate range
    lv_chart_set_range(_chart, LV_CHART_AXIS_PRIMARY_Y, yMin, yMax);

    for (int i = 0; i < HOURS; ++i) {
        if (i < (int)n) {
            const auto& h = hours[i];
            lv_chart_set_value_by_id(_chart, _serTemp, i, (int32_t)lroundf(h.temp));
            lv_chart_set_value_by_id(_chart, _serRain, i, (int32_t)h.rainPct);
            lv_chart_set_value_by_id(_chart, _serSnow, i, (int32_t)h.snowPct);
        } else {
            lv_chart_set_value_by_id(_chart, _serTemp, i, LV_CHART_POINT_NONE);
            lv_chart_set_value_by_id(_chart, _serRain, i, LV_CHART_POINT_NONE);
            lv_chart_set_value_by_id(_chart, _serSnow, i, LV_CHART_POINT_NONE);
        }
    }
    lv_chart_refresh(_chart);

    // ---- Hour labels under the chart (every 3 h) ----
    // x position = pad_left + i/(HOURS-1) * (inner_w - 2*pad)
    const int innerPlotW = CHART_INNER_W - 2 * CHART_X_MARGIN;
    for (int li = 0; li < HOUR_LABEL_COUNT; ++li) {
        if (!_hourLabels[li]) continue;
        const int hourIdx = li * HOUR_LABEL_STEP;
        if (hourIdx < (int)n) {
            time_t ts = hours[hourIdx].ts;
            struct tm tmv;
            localtime_r(&ts, &tmv);
            snprintf(buf, sizeof(buf), "%02d:00", tmv.tm_hour);
            lv_label_set_text(_hourLabels[li], buf);
            lv_obj_clear_flag(_hourLabels[li], LV_OBJ_FLAG_HIDDEN);
        } else if (hourIdx == HOURS && n == HOURS) {
            // Rightmost tick: one hour past the last sample.
            time_t ts = hours[n - 1].ts + 3600;
            struct tm tmv;
            localtime_r(&ts, &tmv);
            snprintf(buf, sizeof(buf), "%02d:00", tmv.tm_hour);
            lv_label_set_text(_hourLabels[li], buf);
            lv_obj_clear_flag(_hourLabels[li], LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(_hourLabels[li], LV_OBJ_FLAG_HIDDEN);
            continue;
        }
        // Position label centered under the data point. The two end-most
        // labels coincide with the chart's plotting area edges.
        const float frac = (HOURS > 1) ? (float)hourIdx / (float)(HOURS - 1) : 0.0f;
        int xPos = CHART_X_MARGIN + (int)(frac * innerPlotW) - 18;
        if (xPos < 0) xPos = 0;
        lv_obj_set_pos(_hourLabels[li], xPos, CHART_INNER_H + 2);
    }
}
