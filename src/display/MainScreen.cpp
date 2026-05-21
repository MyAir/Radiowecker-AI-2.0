#include "MainScreen.h"
#include "../weather/WeatherManager.h"
#include <stdio.h>
#include <math.h>
#include <SD.h>
#include <lvgl.h>  // lv_lock() / lv_unlock()

#include "../serial_safe.h"

// ---------------------------------------------------------------------------
// External fonts
// ---------------------------------------------------------------------------
// ui_font_ms14m — Montserrat Medium 14 px, glyph range 0x20–0xFF (incl.
// German umlauts).  Compiled in from src/display/ui_font_ms14m.c which is
// a copy of SD-Data/assets/ui_font_ms14m.c with the legacy "../ui.h"
// include rewritten to <lvgl.h>.
LV_FONT_DECLARE(ui_font_ms14m)

// ---------------------------------------------------------------------------
// Layout  (800 × 480 landscape)
// ---------------------------------------------------------------------------
static constexpr int SCREEN_W  = 800;
static constexpr int SCREEN_H  = 480;
static constexpr int STATUS_H  = 28;   // top status bar
static constexpr int LEFT_W    = 580;  // clock + sensor area
static constexpr int RIGHT_W   = SCREEN_W - LEFT_W;   // 220 — weather panel
static constexpr int CLOCK_H   = 370;  // clock / date area height
static constexpr int SENSOR_H  = SCREEN_H - STATUS_H - CLOCK_H;  // 82

// Weather tiles — 4 stacked inside the right panel (height = 452 px)
static constexpr int WT_GAP    = 4;
static constexpr int WT_CURR_H = 140;  // current weather tile
static constexpr int WT_FORE_H =
    (SCREEN_H - STATUS_H - WT_CURR_H - 3 * WT_GAP) / 3;  // 100 each

// ---------------------------------------------------------------------------
// Colours
// ---------------------------------------------------------------------------
static constexpr uint32_t C_BG      = 0x000000;  // screen background
static constexpr uint32_t C_STATBG  = 0x1A1A1A;  // status bar background
static constexpr uint32_t C_STATTXT = 0xAAAAAA;  // status bar text
static constexpr uint32_t C_CLOCK   = 0x8B2020;  // clock / date / alarm text
static constexpr uint32_t C_WTILE   = 0x444444;  // weather tile background
static constexpr uint32_t C_WTXT    = 0xCCCCCC;  // weather tile text
static constexpr uint32_t C_SENSOR  = 0xAAAAAA;  // sensor strip text
static constexpr uint32_t C_DIV     = 0x333333;  // divider lines

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

/** Strip default LVGL styling from a container object. */
static void applyContainerStyle(lv_obj_t* obj) {
    lv_obj_set_style_border_width(obj, 0, 0);
    lv_obj_set_style_pad_all(obj, 0, 0);
    lv_obj_set_style_radius(obj, 0, 0);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
}

// ---------------------------------------------------------------------------
// _germanDay()
// ---------------------------------------------------------------------------
const char* MainScreen::_germanDay(int wday) {
    static const char* const days[7] = {
        "Sonntag", "Montag", "Dienstag", "Mittwoch",
        "Donnerstag", "Freitag", "Samstag"
    };
    return (wday >= 0 && wday <= 6) ? days[wday] : "";
}

// ---------------------------------------------------------------------------
// _buildWeatherTile()
// ---------------------------------------------------------------------------
//
// Layout (per tile):
//
//     +--------------------------------------+
//     |              Title                   |   (top, font 14)
//     |                                      |
//     |       [icon]  21 deg C               |   (centered group)
//     |                                      |
//     |        Sub-info / Regen XX%         |   (bottom, ui_font_ms14m
//     +--------------------------------------+    on the "current" tile so
//                                               umlauts in "Gefühlt" /
//                                               OWM description render)
//
// The icon and the temperature share a transparent flex-row container
// that LVGL sizes to its content and that we place at LV_ALIGN_CENTER —
// so the pair is always centered horizontally AND vertically inside the
// tile, regardless of icon visibility.
void MainScreen::_buildWeatherTile(lv_obj_t* parent, Tile& tile, int yOfs, int h,
                                    const char* title, bool isCurrent) {
    const int tileW = RIGHT_W - 9;  // 4 px margin each side + 1 px divider

    lv_obj_t* root = lv_obj_create(parent);
    tile.root = root;
    lv_obj_set_pos(root, 4, yOfs);
    lv_obj_set_size(root, tileW, h);
    lv_obj_set_style_bg_color(root, lv_color_hex(C_WTILE), 0);
    lv_obj_set_style_bg_opa(root, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(root, 0, 0);
    lv_obj_set_style_radius(root, 6, 0);
    lv_obj_set_style_pad_all(root, 6, 0);
    lv_obj_clear_flag(root, LV_OBJ_FLAG_SCROLLABLE);

    // ---- Title (top) -------------------------------------------------------
    lv_obj_t* lblTitle = lv_label_create(root);
    lv_label_set_text(lblTitle, title);
    lv_obj_set_style_text_font(lblTitle, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lblTitle, lv_color_hex(C_WTXT), 0);
    lv_obj_align(lblTitle, LV_ALIGN_TOP_MID, 0, 0);

    // ---- Centered icon+temp group -----------------------------------------
    lv_obj_t* center = lv_obj_create(root);
    lv_obj_remove_style_all(center);
    lv_obj_set_size(center, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_layout(center, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(center, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(center,
                          LV_FLEX_ALIGN_CENTER,    // main axis (row)
                          LV_FLEX_ALIGN_CENTER,    // cross axis (vertical)
                          LV_FLEX_ALIGN_CENTER);   // tracks
    lv_obj_set_style_pad_column(center, 8, 0);
    lv_obj_clear_flag(center, LV_OBJ_FLAG_SCROLLABLE);
    // Pull the group slightly down to leave room for the title above and
    // the sub line below; LV_ALIGN_CENTER would otherwise sit too high
    // because the title eats space at the top.
    lv_obj_align(center, LV_ALIGN_CENTER, 0, isCurrent ? 4 : 2);

    // Icon — hidden until a PNG is loaded for it.
    tile.icon = lv_image_create(center);
    lv_obj_add_flag(tile.icon, LV_OBJ_FLAG_HIDDEN);

    // Temperature
    tile.lblTemp = lv_label_create(center);
    lv_label_set_text(tile.lblTemp, "--\xc2\xb0\x43");
    lv_obj_set_style_text_font(tile.lblTemp,
        isCurrent ? &lv_font_montserrat_32 : &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(tile.lblTemp, lv_color_hex(C_WTXT), 0);

    // ---- Sub line (bottom) ------------------------------------------------
    tile.lblSub = lv_label_create(root);
    lv_label_set_text(tile.lblSub, isCurrent ? "" : "Regen: --%");
    // Current tile uses ui_font_ms14m so the German umlauts in "Gefühlt"
    // and in OWM's localized weather description (e.g. "klarer Himmel",
    // "leichter Regen") render correctly. The forecast tiles only show
    // ASCII ("Regen: NN%") so the standard Montserrat 14 is enough.
    lv_obj_set_style_text_font(tile.lblSub,
        isCurrent ? &ui_font_ms14m : &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(tile.lblSub, lv_color_hex(C_WTXT), 0);
    lv_obj_set_style_text_align(tile.lblSub, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(tile.lblSub, tileW - 12);
    lv_label_set_long_mode(tile.lblSub, LV_LABEL_LONG_DOT);
    lv_obj_align(tile.lblSub, LV_ALIGN_BOTTOM_MID, 0, 0);
}

// ---------------------------------------------------------------------------
// create()
// ---------------------------------------------------------------------------
void MainScreen::create() {
    lv_lock();
    lv_obj_t* scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, lv_color_hex(C_BG), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    // -----------------------------------------------------------------------
    // Status bar
    // -----------------------------------------------------------------------
    lv_obj_t* statusBar = lv_obj_create(scr);
    lv_obj_set_pos(statusBar, 0, 0);
    lv_obj_set_size(statusBar, SCREEN_W, STATUS_H);
    lv_obj_set_style_bg_color(statusBar, lv_color_hex(C_STATBG), 0);
    lv_obj_set_style_bg_opa(statusBar, LV_OPA_COVER, 0);
    applyContainerStyle(statusBar);

    _lblWifiName = lv_label_create(statusBar);
    lv_label_set_text(_lblWifiName, "WiFi: Not Connected");
    lv_obj_set_style_text_font(_lblWifiName, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(_lblWifiName, lv_color_hex(C_STATTXT), 0);
    lv_obj_align(_lblWifiName, LV_ALIGN_LEFT_MID, 8, 0);

    _lblIP = lv_label_create(statusBar);
    lv_label_set_text(_lblIP, "IP: ---");
    lv_obj_set_style_text_font(_lblIP, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(_lblIP, lv_color_hex(C_STATTXT), 0);
    lv_obj_align(_lblIP, LV_ALIGN_CENTER, 0, 0);

    _lblWifiQuality = lv_label_create(statusBar);
    lv_label_set_text(_lblWifiQuality, "0 %");
    lv_obj_set_style_text_font(_lblWifiQuality, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(_lblWifiQuality, lv_color_hex(C_STATTXT), 0);
    lv_obj_align(_lblWifiQuality, LV_ALIGN_RIGHT_MID, -8, 0);

    // -----------------------------------------------------------------------
    // Clock panel  (left side, below status bar)
    // -----------------------------------------------------------------------
    lv_obj_t* clockPanel = lv_obj_create(scr);
    lv_obj_set_pos(clockPanel, 0, STATUS_H);
    lv_obj_set_size(clockPanel, LEFT_W, CLOCK_H);
    lv_obj_set_style_bg_opa(clockPanel, LV_OPA_TRANSP, 0);
    applyContainerStyle(clockPanel);

    _lblDate = lv_label_create(clockPanel);
    lv_label_set_text(_lblDate, "--. --. ----");
    lv_obj_set_style_text_font(_lblDate, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(_lblDate, lv_color_hex(C_CLOCK), 0);
    lv_obj_set_width(_lblDate, LEFT_W);
    lv_obj_set_style_text_align(_lblDate, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(_lblDate, LV_ALIGN_TOP_MID, 0, 50);

    _lblTime = lv_label_create(clockPanel);
    lv_label_set_text(_lblTime, "--:--:--");
    lv_obj_set_style_text_font(_lblTime, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(_lblTime, lv_color_hex(C_CLOCK), 0);
    // Fixed width prevents LVGL from auto-resizing the label when digit widths
    // change (proportional font: '1' is narrower than '0'-'9').  Without this,
    // LV_ALIGN_TOP_MID recalculates x every second, causing horizontal jitter.
    lv_obj_set_width(_lblTime, LEFT_W);
    lv_obj_set_style_text_align(_lblTime, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(_lblTime, LV_ALIGN_TOP_MID, 0, 110);

    _lblNextAlarm = lv_label_create(clockPanel);
    lv_label_set_text(_lblNextAlarm,
        "Naechster Alarm: Donnerstag 31.12.2025 22:22");
    lv_obj_set_style_text_font(_lblNextAlarm, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(_lblNextAlarm, lv_color_hex(C_CLOCK), 0);
    lv_obj_align(_lblNextAlarm, LV_ALIGN_BOTTOM_MID, 0, -15);

    // -----------------------------------------------------------------------
    // Temporary debug controls: 3 buttons + volume slider below the clock
    // -----------------------------------------------------------------------
    {
        static const char* const btnLabels[3] = { "MP3 SD", "SRF 3", "Stop" };
        ButtonCallback* const    btnTargets[3] = {
            &_onPlayFile, &_onPlayStream, &_onStop
        };
        const int BTN_W   = 140;
        const int BTN_H   = 44;
        const int BTN_GAP = 16;
        const int totalW  = 3 * BTN_W + 2 * BTN_GAP;
        const int xStart  = (LEFT_W - totalW) / 2;
        const int yBtn    = 200;

        for (int i = 0; i < 3; i++) {
            lv_obj_t* btn = lv_button_create(clockPanel);
            lv_obj_set_size(btn, BTN_W, BTN_H);
            lv_obj_set_pos(btn, xStart + i * (BTN_W + BTN_GAP), yBtn);
            lv_obj_set_style_radius(btn, 6, 0);
            lv_obj_add_event_cb(btn, _btnEventCb, LV_EVENT_CLICKED,
                                btnTargets[i]);

            lv_obj_t* lbl = lv_label_create(btn);
            lv_label_set_text(lbl, btnLabels[i]);
            lv_obj_set_style_text_font(lbl, &lv_font_montserrat_16, 0);
            lv_obj_center(lbl);
        }

        // Volume slider
        const int SL_W = totalW;
        const int ySl  = yBtn + BTN_H + 24;

        lv_obj_t* lblVol = lv_label_create(clockPanel);
        lv_label_set_text(lblVol, "Vol");
        lv_obj_set_style_text_font(lblVol, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(lblVol, lv_color_hex(C_SENSOR), 0);
        lv_obj_set_pos(lblVol, xStart - 32, ySl - 4);

        _slVolume = lv_slider_create(clockPanel);
        lv_obj_set_size(_slVolume, SL_W, 14);
        lv_obj_set_pos(_slVolume, xStart, ySl);
        lv_slider_set_range(_slVolume, 0, 21);
        lv_slider_set_value(_slVolume, 10, LV_ANIM_OFF);
        lv_obj_add_event_cb(_slVolume, _sliderEventCb,
                            LV_EVENT_VALUE_CHANGED, this);
    }

    // -----------------------------------------------------------------------
    // Horizontal divider  (clock panel / sensor strip)
    // -----------------------------------------------------------------------
    lv_obj_t* hDiv = lv_obj_create(scr);
    lv_obj_set_pos(hDiv, 0, STATUS_H + CLOCK_H);
    lv_obj_set_size(hDiv, LEFT_W, 1);
    lv_obj_set_style_bg_color(hDiv, lv_color_hex(C_DIV), 0);
    lv_obj_set_style_bg_opa(hDiv, LV_OPA_COVER, 0);
    applyContainerStyle(hDiv);

    // -----------------------------------------------------------------------
    // Sensor strip  (bottom-left, below clock panel)
    // -----------------------------------------------------------------------
    lv_obj_t* sensorStrip = lv_obj_create(scr);
    lv_obj_set_pos(sensorStrip, 0, STATUS_H + CLOCK_H + 1);
    lv_obj_set_size(sensorStrip, LEFT_W, SENSOR_H - 1);
    lv_obj_set_style_bg_opa(sensorStrip, LV_OPA_TRANSP, 0);
    applyContainerStyle(sensorStrip);

    static const char* sNames[]  = { "TEMPERATURE", "HUMIDITY", "CO2", "TVOC" };
    static const char* sValues[] = { "--\xc2\xb0\x43", "--%", "---", "---" };
    lv_obj_t** const valueSlots[4] = { &_lblTemp, &_lblHum, &_lblCO2, &_lblTVOC };
    const int COL_W = LEFT_W / 4;  // 145 px per column

    for (int i = 0; i < 4; i++) {
        lv_obj_t* lblN = lv_label_create(sensorStrip);
        lv_label_set_text(lblN, sNames[i]);
        lv_obj_set_style_text_font(lblN, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(lblN, lv_color_hex(C_SENSOR), 0);
        lv_obj_set_pos(lblN, i * COL_W, 10);
        lv_obj_set_width(lblN, COL_W);
        lv_obj_set_style_text_align(lblN, LV_TEXT_ALIGN_CENTER, 0);

        lv_obj_t* lblV = lv_label_create(sensorStrip);
        lv_label_set_text(lblV, sValues[i]);
        lv_obj_set_style_text_font(lblV, &lv_font_montserrat_20, 0);
        lv_obj_set_style_text_color(lblV, lv_color_hex(C_SENSOR), 0);
        lv_obj_set_pos(lblV, i * COL_W, 34);
        lv_obj_set_width(lblV, COL_W);
        lv_obj_set_style_text_align(lblV, LV_TEXT_ALIGN_CENTER, 0);
        *valueSlots[i] = lblV;
    }

    // -----------------------------------------------------------------------
    // Vertical divider  (left area / right weather panel)
    // -----------------------------------------------------------------------
    lv_obj_t* vDiv = lv_obj_create(scr);
    lv_obj_set_pos(vDiv, LEFT_W, STATUS_H);
    lv_obj_set_size(vDiv, 1, SCREEN_H - STATUS_H);
    lv_obj_set_style_bg_color(vDiv, lv_color_hex(C_DIV), 0);
    lv_obj_set_style_bg_opa(vDiv, LV_OPA_COVER, 0);
    applyContainerStyle(vDiv);

    // -----------------------------------------------------------------------
    // Weather panel  (right side, full height below status bar)
    // -----------------------------------------------------------------------
    lv_obj_t* weatherPanel = lv_obj_create(scr);
    lv_obj_set_pos(weatherPanel, LEFT_W + 1, STATUS_H);
    lv_obj_set_size(weatherPanel, RIGHT_W - 1, SCREEN_H - STATUS_H);
    lv_obj_set_style_bg_opa(weatherPanel, LV_OPA_TRANSP, 0);
    applyContainerStyle(weatherPanel);

    int tileY = 0;
    _buildWeatherTile(weatherPanel, _wCur,  tileY, WT_CURR_H, "Aktuelles Wetter", true);
    tileY += WT_CURR_H + WT_GAP;
    _buildWeatherTile(weatherPanel, _wMorn, tileY, WT_FORE_H, "Vormittag",   false);
    tileY += WT_FORE_H + WT_GAP;
    _buildWeatherTile(weatherPanel, _wAft,  tileY, WT_FORE_H, "Nachmittag", false);
    tileY += WT_FORE_H + WT_GAP;
    _buildWeatherTile(weatherPanel, _wTom,  tileY, WT_FORE_H, "Morgen",     false);
    lv_unlock();
}

// ---------------------------------------------------------------------------
// Debug audio control callbacks + setVolume()
// ---------------------------------------------------------------------------
void MainScreen::_btnEventCb(lv_event_t* e) {
    auto* cbp = static_cast<ButtonCallback*>(lv_event_get_user_data(e));
    if (cbp && *cbp) (*cbp)();
}

void MainScreen::_sliderEventCb(lv_event_t* e) {
    auto* self = static_cast<MainScreen*>(lv_event_get_user_data(e));
    if (!self) return;
    lv_obj_t* sl = static_cast<lv_obj_t*>(lv_event_get_target(e));
    int v = lv_slider_get_value(sl);
    if (v < 0) v = 0;
    if (v > 21) v = 21;
    if (self->_onVolume) self->_onVolume(static_cast<uint8_t>(v));
}

void MainScreen::setVolume(uint8_t vol) {
    if (!_slVolume) return;
    if (vol > 21) vol = 21;
    lv_lock();
    lv_slider_set_value(_slVolume, vol, LV_ANIM_OFF);
    lv_unlock();
}

// ---------------------------------------------------------------------------
// updateTime()
// ---------------------------------------------------------------------------
void MainScreen::updateTime(const struct tm& t) {
    if (!_lblDate || !_lblTime) return;

    // Only call lv_label_set_text() when the displayed value actually changes
    // to avoid redundant LVGL dirty-region marks.
    static int s_last_sec  = -1;
    static int s_last_min  = -1;
    static int s_last_hour = -1;
    static int s_last_mday = -1;

    const bool time_changed = (t.tm_hour != s_last_hour || t.tm_min != s_last_min
                                                         || t.tm_sec != s_last_sec);
    const bool date_changed = (t.tm_mday != s_last_mday);

    if (!time_changed && !date_changed) return;

    char buf[64];
    lv_lock();
    if (date_changed) {
        snprintf(buf, sizeof(buf), "%s %02d.%02d.%04d",
                 _germanDay(t.tm_wday),
                 t.tm_mday, t.tm_mon + 1, t.tm_year + 1900);
        lv_label_set_text(_lblDate, buf);
        s_last_mday = t.tm_mday;
    }
    if (time_changed) {
        snprintf(buf, sizeof(buf), "%02d:%02d:%02d", t.tm_hour, t.tm_min, t.tm_sec);
        lv_label_set_text(_lblTime, buf);
        s_last_hour = t.tm_hour;
        s_last_min  = t.tm_min;
        s_last_sec  = t.tm_sec;
    }
    lv_unlock();
}

// ---------------------------------------------------------------------------
// updateWifi()
// ---------------------------------------------------------------------------
void MainScreen::updateWifi(const char* ssid, const char* ip, int quality) {
    if (!_lblWifiName || !_lblIP || !_lblWifiQuality) return;

    // Change-guards: lv_label_set_text() always marks the label dirty even
    // when the text is identical, causing a 44 KB Cache_WriteBack_Addr every
    // second for rows 0-27.  Only call it when the content actually changes.
    static char s_last_ssid[64]  = {};
    static char s_last_ip[32]    = {};
    static int  s_last_quality   = -1;

    char buf_name[80], buf_ip[48], buf_qual[16];
    snprintf(buf_name, sizeof(buf_name), "WiFi: %s", ssid);
    snprintf(buf_ip,   sizeof(buf_ip),   "IP: %s",   ip);
    snprintf(buf_qual, sizeof(buf_qual), "%d %%",     quality);

    const bool name_ch = (strncmp(buf_name, s_last_ssid, sizeof(s_last_ssid)) != 0);
    const bool ip_ch   = (strncmp(buf_ip,   s_last_ip,   sizeof(s_last_ip))   != 0);
    const bool qual_ch = (quality != s_last_quality);
    if (!name_ch && !ip_ch && !qual_ch) return;

    lv_lock();
    if (name_ch) { lv_label_set_text(_lblWifiName,    buf_name); strncpy(s_last_ssid, buf_name, sizeof(s_last_ssid) - 1); }
    if (ip_ch)   { lv_label_set_text(_lblIP,          buf_ip);   strncpy(s_last_ip,   buf_ip,   sizeof(s_last_ip)   - 1); }
    if (qual_ch) {
        lv_label_set_text(_lblWifiQuality, buf_qual);
        // Color rules from Radiowecker_EEZ_AI (UIManager::updateWiFiStatusUI).
        uint32_t qcol;
        if      (quality < 30) qcol = 0xFF0000;  // poor   — red
        else if (quality < 50) qcol = 0xFF8000;  // weak   — orange
        else if (quality < 70) qcol = 0xFFFF00;  // medium — yellow
        else                   qcol = 0x00FF00;  // good   — green
        lv_obj_set_style_text_color(_lblWifiQuality, lv_color_hex(qcol), 0);
        s_last_quality = quality;
    }
    lv_unlock();
}

// ---------------------------------------------------------------------------
// updateSensors()
// ---------------------------------------------------------------------------
// Color thresholds taken verbatim from Radiowecker_EEZ_AI/src/UIManager.cpp
// (updateTemperature / updateHumidity / updateCO2 / updateTVOC).
void MainScreen::updateSensors(float temp, float hum, uint16_t co2, uint16_t tvoc) {
    if (!_lblTemp || !_lblHum || !_lblCO2 || !_lblTVOC) return;

    static float    s_last_temp = -1000.0f;
    static float    s_last_hum  = -1.0f;
    static uint16_t s_last_co2  = 0xFFFF;
    static uint16_t s_last_tvoc = 0xFFFF;
    static bool     s_first     = true;

    char buf[16];
    lv_lock();

    // ---- Temperature ----
    if (s_first || fabsf(temp - s_last_temp) >= 0.1f) {
        snprintf(buf, sizeof(buf), "%.1f\xc2\xb0\x43", temp);
        lv_label_set_text(_lblTemp, buf);
        uint32_t c;
        if      (temp <  16.0f) c = 0x00AFFF;  // cold        — blue
        else if (temp <= 23.0f) c = 0x00FF00;  // comfortable — green
        else if (temp <= 26.0f) c = 0xFF9A00;  // warm        — orange
        else                    c = 0xFF0000;  // hot         — red
        lv_obj_set_style_text_color(_lblTemp, lv_color_hex(c), 0);
        s_last_temp = temp;
    }

    // ---- Humidity ----
    if (s_first || fabsf(hum - s_last_hum) >= 1.0f) {
        snprintf(buf, sizeof(buf), "%.0f%%", hum);
        lv_label_set_text(_lblHum, buf);
        uint32_t c;
        if      (hum < 40.0f) c = 0xFFD700;  // dry     — yellow
        else if (hum <= 60.0f) c = 0x00FF00; // optimal — green
        else                   c = 0x00AFFF; // humid   — blue
        lv_obj_set_style_text_color(_lblHum, lv_color_hex(c), 0);
        s_last_hum = hum;
    }

    // ---- CO2 ----
    if (s_first || co2 != s_last_co2) {
        snprintf(buf, sizeof(buf), "%u ppm", (unsigned)co2);
        lv_label_set_text(_lblCO2, buf);
        uint32_t c;
        if      (co2 <  800) c = 0x00FF00;  // excellent — green
        else if (co2 < 1200) c = 0xFFD700;  // good      — yellow
        else if (co2 < 1800) c = 0xFF9A00;  // moderate  — orange
        else                 c = 0xFF0000;  // poor      — red
        lv_obj_set_style_text_color(_lblCO2, lv_color_hex(c), 0);
        s_last_co2 = co2;
    }

    // ---- TVOC ----
    if (s_first || tvoc != s_last_tvoc) {
        snprintf(buf, sizeof(buf), "%u ppb", (unsigned)tvoc);
        lv_label_set_text(_lblTVOC, buf);
        uint32_t c;
        if      (tvoc < 100) c = 0x00FF00;  // excellent — green
        else if (tvoc < 300) c = 0xFFD700;  // good      — yellow
        else if (tvoc < 500) c = 0xFF9A00;  // moderate  — orange
        else                 c = 0xFF0000;  // poor      — red
        lv_obj_set_style_text_color(_lblTVOC, lv_color_hex(c), 0);
        s_last_tvoc = tvoc;
    }

    s_first = false;
    lv_unlock();
}

// ---------------------------------------------------------------------------
// Weather icon cache
// ---------------------------------------------------------------------------
//
// Each OpenWeatherMap icon code (e.g. "01d", "10n") maps to a 50×50 PNG on
// the SD card under /assets/weather_icons/.  We load each PNG file once
// into a PSRAM-backed lv_image_dsc_t so subsequent updates are instant
// and don't hammer the SD bus.  The buffers live for the lifetime of the
// program — there are at most 18 icons (~3 KB each ≈ 60 KB).
namespace {

struct IconCacheEntry {
    char            code[8];   // e.g. "01d"
    uint8_t*        bytes;     // PSRAM-allocated PNG file content
    size_t          len;
    lv_image_dsc_t  dsc;
};

constexpr int ICON_CACHE_MAX = 20;
static IconCacheEntry s_iconCache[ICON_CACHE_MAX];
static int            s_iconCacheCount = 0;

static const lv_image_dsc_t* loadIcon(const char* code) {
    if (!code || code[0] == '\0') return nullptr;

    // Cache hit?
    for (int i = 0; i < s_iconCacheCount; ++i) {
        if (strncmp(s_iconCache[i].code, code, sizeof(s_iconCache[i].code)) == 0) {
            return &s_iconCache[i].dsc;
        }
    }
    if (s_iconCacheCount >= ICON_CACHE_MAX) {
        serial_safe_println("[Weather] icon cache full");
        return nullptr;
    }

    char path[48];
    snprintf(path, sizeof(path), "/assets/weather_icons/%s.png", code);
    File f = SD.open(path, FILE_READ);
    if (!f) {
        serial_safe_printf("[Weather] icon file missing: %s\n", path);
        return nullptr;
    }
    const size_t len = f.size();
    if (len < 16 || len > 32 * 1024) {
        serial_safe_printf("[Weather] icon size suspicious: %u\n", (unsigned)len);
        f.close();
        return nullptr;
    }
    uint8_t* buf = (uint8_t*)ps_malloc(len);
    if (!buf) {
        serial_safe_println("[Weather] PSRAM alloc for icon failed");
        f.close();
        return nullptr;
    }
    const size_t rd = f.read(buf, len);
    f.close();
    if (rd != len) {
        serial_safe_printf("[Weather] icon short read: %u/%u\n",
                           (unsigned)rd, (unsigned)len);
        free(buf);
        return nullptr;
    }

    IconCacheEntry& e = s_iconCache[s_iconCacheCount++];
    strncpy(e.code, code, sizeof(e.code) - 1);
    e.code[sizeof(e.code) - 1] = '\0';
    e.bytes = buf;
    e.len   = len;

    // Hand the raw PNG bytes to LVGL's lodepng decoder via a VARIABLE
    // image descriptor.  Width/height/cf are populated by the decoder
    // through its info_cb when first used; we only need data + size.
    e.dsc.header.cf       = LV_COLOR_FORMAT_RAW_ALPHA;
    e.dsc.header.w        = 0;
    e.dsc.header.h        = 0;
    e.dsc.header.stride   = 0;
    e.dsc.data_size       = (uint32_t)len;
    e.dsc.data            = buf;

    return &e.dsc;
}

} // namespace

// ---------------------------------------------------------------------------
// updateWeather()
// ---------------------------------------------------------------------------
//
// Push a fresh WeatherManager snapshot into the four tiles.  Called
// once after each successful API poll (every 5 minutes) — UI churn is
// minimal so we just rewrite all labels unconditionally.
void MainScreen::updateWeather(const WeatherManager& w) {
    if (!w.hasData()) return;

    auto applySlot = [](Tile& tile, const WeatherManager::Slot& s,
                        bool isCurrent) {
        if (!tile.root) return;

        // Icon
        if (s.valid && strncmp(tile.iconCode, s.icon, sizeof(tile.iconCode)) != 0) {
            const lv_image_dsc_t* dsc = loadIcon(s.icon);
            if (dsc) {
                lv_image_set_src(tile.icon, dsc);
                lv_obj_clear_flag(tile.icon, LV_OBJ_FLAG_HIDDEN);
                strncpy(tile.iconCode, s.icon, sizeof(tile.iconCode) - 1);
                tile.iconCode[sizeof(tile.iconCode) - 1] = '\0';
            }
        }

        // Temperature
        char buf[64];
        if (s.valid) {
            snprintf(buf, sizeof(buf), "%.0f\xc2\xb0\x43", s.temp);
        } else {
            snprintf(buf, sizeof(buf), "--\xc2\xb0\x43");
        }
        lv_label_set_text(tile.lblTemp, buf);

        // Sub line
        if (isCurrent) {
            // "Gefühlt 23°C" + description on a second line
            // Use UTF-8 escapes for ä/ü so source stays ASCII-safe.
            if (s.valid) {
                snprintf(buf, sizeof(buf),
                         "Gef\xc3\xbchlt: %.0f\xc2\xb0\x43\n%s",
                         s.feels, s.desc);
            } else {
                snprintf(buf, sizeof(buf), "Keine Daten");
            }
        } else {
            if (s.valid && s.pop >= 0) {
                snprintf(buf, sizeof(buf), "Regen: %d%%", s.pop);
            } else {
                snprintf(buf, sizeof(buf), "Regen: --%%");
            }
        }
        lv_label_set_text(tile.lblSub, buf);
    };

    lv_lock();
    applySlot(_wCur,  w.current(),   true);
    applySlot(_wMorn, w.morning(),   false);
    applySlot(_wAft,  w.afternoon(), false);
    applySlot(_wTom,  w.tomorrow(),  false);
    lv_unlock();
}
