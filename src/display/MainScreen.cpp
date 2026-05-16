#include "MainScreen.h"
#include <stdio.h>

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
lv_obj_t* MainScreen::_buildWeatherTile(lv_obj_t* parent, int yOfs, int h,
                                          const char* title, bool isCurrent) {
    const int tileW = RIGHT_W - 9;  // 4 px margin each side + 1 px divider

    lv_obj_t* tile = lv_obj_create(parent);
    lv_obj_set_pos(tile, 4, yOfs);
    lv_obj_set_size(tile, tileW, h);
    lv_obj_set_style_bg_color(tile, lv_color_hex(C_WTILE), 0);
    lv_obj_set_style_bg_opa(tile, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(tile, 0, 0);
    lv_obj_set_style_radius(tile, 6, 0);
    lv_obj_set_style_pad_all(tile, 6, 0);
    lv_obj_clear_flag(tile, LV_OBJ_FLAG_SCROLLABLE);

    // Title
    lv_obj_t* lblTitle = lv_label_create(tile);
    lv_label_set_text(lblTitle, title);
    lv_obj_set_style_text_font(lblTitle, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lblTitle, lv_color_hex(C_WTXT), 0);
    lv_obj_align(lblTitle, LV_ALIGN_TOP_MID, 0, 0);

    // Temperature
    lv_obj_t* lblTemp = lv_label_create(tile);
    lv_label_set_text(lblTemp, isCurrent ? "0.0\xc2\xb0\x43" : "--\xc2\xb0\x43");
    lv_obj_set_style_text_font(lblTemp,
        isCurrent ? &lv_font_montserrat_24 : &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(lblTemp, lv_color_hex(C_WTXT), 0);
    lv_obj_align(lblTemp, LV_ALIGN_CENTER, 0, 0);

    // Sub-info
    lv_obj_t* lblSub = lv_label_create(tile);
    lv_label_set_text(lblSub,
        isCurrent ? "Gefuehlt: --\xc2\xb0\x43\nKeine Daten" : "Regen: --%");
    lv_obj_set_style_text_font(lblSub, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lblSub, lv_color_hex(C_WTXT), 0);
    lv_obj_set_style_text_align(lblSub, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(lblSub, LV_ALIGN_BOTTOM_MID, 0, 0);

    return tile;
}

// ---------------------------------------------------------------------------
// create()
// ---------------------------------------------------------------------------
void MainScreen::create() {
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
    lv_obj_align(_lblDate, LV_ALIGN_TOP_MID, 0, 50);

    _lblTime = lv_label_create(clockPanel);
    lv_label_set_text(_lblTime, "--:--:--");
    lv_obj_set_style_text_font(_lblTime, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(_lblTime, lv_color_hex(C_CLOCK), 0);
    lv_obj_align(_lblTime, LV_ALIGN_TOP_MID, 0, 110);

    _lblNextAlarm = lv_label_create(clockPanel);
    lv_label_set_text(_lblNextAlarm,
        "Naechster Alarm: Donnerstag 31.12.2025 22:22");
    lv_obj_set_style_text_font(_lblNextAlarm, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(_lblNextAlarm, lv_color_hex(C_CLOCK), 0);
    lv_obj_align(_lblNextAlarm, LV_ALIGN_BOTTOM_MID, 0, -15);

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
    _buildWeatherTile(weatherPanel, tileY, WT_CURR_H, "Aktuelles Wetter", true);
    tileY += WT_CURR_H + WT_GAP;
    _buildWeatherTile(weatherPanel, tileY, WT_FORE_H, "Vormittag", false);
    tileY += WT_FORE_H + WT_GAP;
    _buildWeatherTile(weatherPanel, tileY, WT_FORE_H, "Nachmittag", false);
    tileY += WT_FORE_H + WT_GAP;
    _buildWeatherTile(weatherPanel, tileY, WT_FORE_H, "Nacht", false);
}

// ---------------------------------------------------------------------------
// updateTime()
// ---------------------------------------------------------------------------
void MainScreen::updateTime(const struct tm& t) {
    if (!_lblDate || !_lblTime) return;

    char buf[64];
    snprintf(buf, sizeof(buf), "%s %02d.%02d.%04d",
             _germanDay(t.tm_wday),
             t.tm_mday, t.tm_mon + 1, t.tm_year + 1900);
    lv_label_set_text(_lblDate, buf);

    snprintf(buf, sizeof(buf), "%02d:%02d:%02d",
             t.tm_hour, t.tm_min, t.tm_sec);
    lv_label_set_text(_lblTime, buf);
}

// ---------------------------------------------------------------------------
// updateWifi()
// ---------------------------------------------------------------------------
void MainScreen::updateWifi(const char* ssid, const char* ip, int quality) {
    if (!_lblWifiName || !_lblIP || !_lblWifiQuality) return;

    char buf[80];
    snprintf(buf, sizeof(buf), "WiFi: %s", ssid);
    lv_label_set_text(_lblWifiName, buf);

    snprintf(buf, sizeof(buf), "IP: %s", ip);
    lv_label_set_text(_lblIP, buf);

    snprintf(buf, sizeof(buf), "%d %%", quality);
    lv_label_set_text(_lblWifiQuality, buf);
}
