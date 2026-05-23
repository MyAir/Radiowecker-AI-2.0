#include "MainScreen.h"
#include <stdio.h>
#include <math.h>
#include <lvgl.h>  // lv_lock() / lv_unlock()

#include "../serial_safe.h"

// ---------------------------------------------------------------------------
// External fonts
// ---------------------------------------------------------------------------
// ui_font_ms14m  — Montserrat Medium  14 px, 0x20–0xFF (incl. German umlauts)
// ui_font_ms24m  — Montserrat Medium  24 px, 0x20–0xFF (incl. German umlauts)
// ui_font_ms28m  — Montserrat Medium  28 px, 0x20–0xFF (incl. German umlauts)
// ui_font_ms36m  — Montserrat Medium  36 px, 0x20–0xFF (incl. German umlauts)
// ui_font_ms80m  — Montserrat Medium  80 px, 0x20–0x7F (ASCII — digits + colon)
// ui_font_ms120m — Montserrat Medium 120 px, 0x20–0x7F (ASCII — digits + colon)
LV_FONT_DECLARE(ui_font_ms14m)
LV_FONT_DECLARE(ui_font_ms24m)
LV_FONT_DECLARE(ui_font_ms28m)
LV_FONT_DECLARE(ui_font_ms36m)
LV_FONT_DECLARE(ui_font_ms80m)
LV_FONT_DECLARE(ui_font_ms120m)

// ---------------------------------------------------------------------------
// Layout  (800 × 480 landscape)
// ---------------------------------------------------------------------------
static constexpr int SCREEN_W  = 800;
static constexpr int SCREEN_H  = 480;
static constexpr int STATUS_H  = 28;   // top status bar
static constexpr int SENSOR_H  = 44;   // sensor strip at bottom
static constexpr int CONTENT_H = SCREEN_H - STATUS_H - SENSOR_H;  // 408

// ---------------------------------------------------------------------------
// Colours  (warm-amber palette for bedroom dim mode)
// ---------------------------------------------------------------------------
static constexpr uint32_t C_BG         = 0x000000;  // screen background
static constexpr uint32_t C_STATBG     = 0x121212;  // status bar background
static constexpr uint32_t C_STATTXT    = 0x646464;  // status bar text
static constexpr uint32_t C_TIME       = 0xC86E0F;  // time — bright amber
static constexpr uint32_t C_DATE       = 0xA05A0C;  // date — medium amber
static constexpr uint32_t C_WKDAY      = 0x6E3C08;  // weekday — dim amber
static constexpr uint32_t C_ALARM_LBL  = 0x6E3C08;  // alarm caption
static constexpr uint32_t C_ALARM_VAL  = 0xBE690E;  // alarm value
static constexpr uint32_t C_SKIP_BORD  = 0x643708;  // Skip button border
static constexpr uint32_t C_SKIP_TXT   = 0x7A4409;  // Skip button text
static constexpr uint32_t C_DIVIDER    = 0x502D05;  // separator line
static constexpr uint32_t C_SENSOR_LBL = 0x6E410A;  // sensor name labels
static constexpr uint32_t C_SENSOR_BG  = 0x0E0E0E;  // sensor strip background

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
// _germanDay() / _germanMonthShort()
// ---------------------------------------------------------------------------
const char* MainScreen::_germanDay(int wday) {
    static const char* const days[7] = {
        "Sonntag", "Montag", "Dienstag", "Mittwoch",
        "Donnerstag", "Freitag", "Samstag"
    };
    return (wday >= 0 && wday <= 6) ? days[wday] : "";
}

const char* MainScreen::_germanMonthShort(int mon) {
    static const char* const months[12] = {
        "Jan.", "Feb.", "M\xc3\xa4" "rz.", "Apr.", "Mai", "Jun.",
        "Jul.", "Aug.", "Sep.", "Okt.", "Nov.", "Dez."
    };
    return (mon >= 0 && mon <= 11) ? months[mon] : "";
}

// ---------------------------------------------------------------------------
// _skipBtnEventCb()
// ---------------------------------------------------------------------------
void MainScreen::_skipBtnEventCb(lv_event_t* e) {
    auto* self = static_cast<MainScreen*>(lv_event_get_user_data(e));
    if (self && self->_onSkipAlarm) self->_onSkipAlarm();
}

void MainScreen::_prevBtnEventCb(lv_event_t* e) {
    auto* self = static_cast<MainScreen*>(lv_event_get_user_data(e));
    if (self && self->_onPrevAlarm) self->_onPrevAlarm();
}

void MainScreen::_settingsBtnEventCb(lv_event_t* e) {
    static uint32_t lastFire = 0;
    const uint32_t now = lv_tick_get();
    if (now - lastFire < 500) return;
    lastFire = now;
    auto* self = static_cast<MainScreen*>(lv_event_get_user_data(e));
    if (self && self->_onSettings) self->_onSettings();
}

void MainScreen::_alarmToggleBtnEventCb(lv_event_t* e) {
    static uint32_t lastFire = 0;
    const uint32_t now = lv_tick_get();
    if (now - lastFire < 500) return;   // debounce: ignore rapid re-fires
    lastFire = now;
    auto* self = static_cast<MainScreen*>(lv_event_get_user_data(e));
    if (self && self->_onAlarmToggle) self->_onAlarmToggle();
}

// ---------------------------------------------------------------------------
// setAlarmEnabled()
// ---------------------------------------------------------------------------
void MainScreen::setAlarmEnabled(bool enabled) {
    if (!_lblAlarmIcon) return;
    // NOTE: no lv_lock() here — this function is called either from setup()
    // (before lv_timer_handler starts) or from within an LVGL event callback
    // (which already holds the FreeRTOS LVGL mutex). Re-locking would deadlock.
    lv_obj_set_style_text_color(_lblAlarmIcon,
        lv_color_hex(enabled ? 0xA05A0C : 0x3A2004), 0);
    if (_lineAlarmStrike) {
        if (enabled) lv_obj_add_flag(_lineAlarmStrike, LV_OBJ_FLAG_HIDDEN);
        else         lv_obj_remove_flag(_lineAlarmStrike, LV_OBJ_FLAG_HIDDEN);
    }
}

// ---------------------------------------------------------------------------
// setNextAlarm()
// ---------------------------------------------------------------------------
void MainScreen::setNextAlarm(const char* text) {
    if (!_lblNextAlarm) return;
    lv_lock();
    lv_label_set_text(_lblNextAlarm, (text && text[0]) ? text : "---");
    lv_unlock();
}

// ---------------------------------------------------------------------------
// ---------------------------------------------------------------------------
// create()
// ---------------------------------------------------------------------------
void MainScreen::create() {
    lv_lock();
    lv_obj_t* scr = lv_scr_act();
    _scr = scr;  // save for screen() getter
    lv_obj_set_style_bg_color(scr, lv_color_hex(C_BG), 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

    // -----------------------------------------------------------------------
    // Status bar  (top, full width)
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
    // Main content panel  (full-width, between status bar and sensor strip)
    // -----------------------------------------------------------------------
    lv_obj_t* panel = lv_obj_create(scr);
    lv_obj_set_pos(panel, 0, STATUS_H);
    lv_obj_set_size(panel, SCREEN_W, CONTENT_H);
    lv_obj_set_style_bg_opa(panel, LV_OPA_TRANSP, 0);
    applyContainerStyle(panel);

    // -----------------------------------------------------------------------
    // Corner icon buttons  (square, as tall as weekday + date combined)
    // -----------------------------------------------------------------------
    //   Settings (top-left)  — cogwheel, no state
    //   Alarm toggle (top-right) — bell, shows enabled/disabled via brightness
    // Both: transparent bg, dim-amber border, lv_font_montserrat_48 symbol.
    {
        const int BTN_SZ = 85;   // height spans weekday (y=20) to date bottom (y≈105)
        const int BTN_Y  = 20;
        const int MARGIN = 15;

        // Settings button
        _btnSettings = lv_button_create(panel);
        lv_obj_set_size(_btnSettings, BTN_SZ, BTN_SZ);
        lv_obj_set_pos(_btnSettings, MARGIN, BTN_Y);
        lv_obj_set_style_radius(_btnSettings, 8, 0);
        lv_obj_set_style_bg_opa(_btnSettings, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_color(_btnSettings, lv_color_hex(C_SKIP_BORD), 0);
        lv_obj_set_style_border_width(_btnSettings, 1, 0);
        lv_obj_add_event_cb(_btnSettings, _settingsBtnEventCb, LV_EVENT_CLICKED, this);

        lv_obj_t* settingsIcon = lv_label_create(_btnSettings);
        lv_label_set_text(settingsIcon, LV_SYMBOL_SETTINGS);
        lv_obj_set_style_text_font(settingsIcon, &lv_font_montserrat_48, 0);
        lv_obj_set_style_text_color(settingsIcon, lv_color_hex(C_WKDAY), 0);
        lv_obj_center(settingsIcon);

        // Alarm toggle button
        _btnAlarmToggle = lv_button_create(panel);
        lv_obj_set_size(_btnAlarmToggle, BTN_SZ, BTN_SZ);
        lv_obj_set_pos(_btnAlarmToggle, SCREEN_W - MARGIN - BTN_SZ, BTN_Y);
        lv_obj_set_style_radius(_btnAlarmToggle, 8, 0);
        lv_obj_set_style_bg_opa(_btnAlarmToggle, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_color(_btnAlarmToggle, lv_color_hex(C_SKIP_BORD), 0);
        lv_obj_set_style_border_width(_btnAlarmToggle, 1, 0);
        lv_obj_add_event_cb(_btnAlarmToggle, _alarmToggleBtnEventCb, LV_EVENT_CLICKED, this);

        _lblAlarmIcon = lv_label_create(_btnAlarmToggle);
        lv_label_set_text(_lblAlarmIcon, LV_SYMBOL_BELL);
        lv_obj_set_style_text_font(_lblAlarmIcon, &lv_font_montserrat_48, 0);
        lv_obj_set_style_text_color(_lblAlarmIcon, lv_color_hex(0x3A2004), 0);  // dim until enabled
        lv_obj_center(_lblAlarmIcon);

        // Diagonal strikethrough — shown when alarm master switch is OFF
        static const lv_point_precise_t strikePoints[] = {{8, 10}, {77, 75}};
        _lineAlarmStrike = lv_line_create(_btnAlarmToggle);
        lv_line_set_points(_lineAlarmStrike, strikePoints, 2);
        lv_obj_set_style_line_color(_lineAlarmStrike, lv_color_hex(0x7A4409), 0);
        lv_obj_set_style_line_width(_lineAlarmStrike, 3, 0);
        lv_obj_set_style_line_rounded(_lineAlarmStrike, true, 0);
        lv_obj_add_flag(_lineAlarmStrike, LV_OBJ_FLAG_HIDDEN);  // enabled by default
    }

    // Weekday name — dim amber, top
    _lblWeekday = lv_label_create(panel);
    lv_label_set_text(_lblWeekday, "---------");
    lv_obj_set_style_text_font(_lblWeekday, &ui_font_ms28m, 0);
    lv_obj_set_style_text_color(_lblWeekday, lv_color_hex(C_WKDAY), 0);
    lv_obj_set_width(_lblWeekday, SCREEN_W);
    lv_obj_set_style_text_align(_lblWeekday, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(_lblWeekday, LV_ALIGN_TOP_MID, 0, 20);

    // Date — medium amber, just below weekday
    _lblDate = lv_label_create(panel);
    lv_label_set_text(_lblDate, "--. --- ----");
    lv_obj_set_style_text_font(_lblDate, &ui_font_ms36m, 0);
    lv_obj_set_style_text_color(_lblDate, lv_color_hex(C_DATE), 0);
    lv_obj_set_width(_lblDate, SCREEN_W);
    lv_obj_set_style_text_align(_lblDate, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(_lblDate, LV_ALIGN_TOP_MID, 0, 63);

    // Time — monospaced: 8 fixed-width cells (H H : M M : S S)
    // Each digit/colon is centered in its own cell so the proportional
    // Montserrat glyphs never shift neighbours when widths vary (e.g. "1").
    // Cell layout: digit=78px, colon=36px → total 540px, start x=130.
    static constexpr struct { int x, w; } kTC[8] = {
        {130,78},{208,78},{286,36},{322,78},{400,78},{478,36},{514,78},{592,78}
    };
    for (int i = 0; i < 8; i++) {
        _timeDigits[i] = lv_label_create(panel);
        lv_label_set_text(_timeDigits[i], (i == 2 || i == 5) ? ":" : "-");
        lv_obj_set_style_text_font(_timeDigits[i], &ui_font_ms120m, 0);
        lv_obj_set_style_text_color(_timeDigits[i], lv_color_hex(C_TIME), 0);
        lv_obj_set_pos(_timeDigits[i], kTC[i].x, 158);
        lv_obj_set_width(_timeDigits[i], kTC[i].w);
        lv_obj_set_style_text_align(_timeDigits[i], LV_TEXT_ALIGN_CENTER, 0);
    }

    // Alarm separator line  (spans the full alarm row: x=50 to x=690)
    lv_obj_t* alarmDiv = lv_obj_create(panel);
    lv_obj_set_pos(alarmDiv, 80, 338);
    lv_obj_set_size(alarmDiv, 640, 1);
    lv_obj_set_style_bg_color(alarmDiv, lv_color_hex(C_DIVIDER), 0);
    lv_obj_set_style_bg_opa(alarmDiv, LV_OPA_COVER, 0);
    applyContainerStyle(alarmDiv);

    // Prev button — leftmost element in alarm row
    {
        const int BTN_W = 80, BTN_H = 34;
        const int BTN_X = 80;
        const int BTN_Y = 351;
        _btnPrevAlarm = lv_button_create(panel);
        lv_obj_set_size(_btnPrevAlarm, BTN_W, BTN_H);
        lv_obj_set_pos(_btnPrevAlarm, BTN_X, BTN_Y);
        lv_obj_set_style_radius(_btnPrevAlarm, 6, 0);
        lv_obj_set_style_bg_opa(_btnPrevAlarm, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_color(_btnPrevAlarm, lv_color_hex(C_SKIP_BORD), 0);
        lv_obj_set_style_border_width(_btnPrevAlarm, 1, 0);
        lv_obj_add_event_cb(_btnPrevAlarm, _prevBtnEventCb, LV_EVENT_CLICKED, this);

        lv_obj_t* prevLbl = lv_label_create(_btnPrevAlarm);
        lv_label_set_text(prevLbl, "Prev");
        lv_obj_set_style_text_font(prevLbl, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(prevLbl, lv_color_hex(C_SKIP_TXT), 0);
        lv_obj_center(prevLbl);
    }

    // Alarm caption — static, left-aligned, after Prev button
    lv_obj_t* lblAlarmCap = lv_label_create(panel);
    lv_label_set_text(lblAlarmCap, "N\xc3\xa4" "chster Alarm:");
    lv_obj_set_style_text_font(lblAlarmCap, &ui_font_ms24m, 0);
    lv_obj_set_style_text_color(lblAlarmCap, lv_color_hex(C_ALARM_LBL), 0);
    lv_obj_set_width(lblAlarmCap, 230);
    lv_obj_set_style_text_align(lblAlarmCap, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_set_pos(lblAlarmCap, 170, 358);

    // Alarm value — dynamic, left-aligned between caption and Next button
    _lblNextAlarm = lv_label_create(panel);
    lv_label_set_text(_lblNextAlarm, "---");
    lv_obj_set_style_text_font(_lblNextAlarm, &ui_font_ms24m, 0);
    lv_obj_set_style_text_color(_lblNextAlarm, lv_color_hex(C_ALARM_VAL), 0);
    lv_obj_set_width(_lblNextAlarm, 220);
    lv_obj_set_pos(_lblNextAlarm, 410, 358);

    // Next button (was Skip) — rounded outline, right end of alarm row
    {
        const int BTN_W = 80, BTN_H = 34;
        const int BTN_X = 640;
        const int BTN_Y = 351;
        _btnSkipAlarm = lv_button_create(panel);
        lv_obj_set_size(_btnSkipAlarm, BTN_W, BTN_H);
        lv_obj_set_pos(_btnSkipAlarm, BTN_X, BTN_Y);
        lv_obj_set_style_radius(_btnSkipAlarm, 6, 0);
        lv_obj_set_style_bg_opa(_btnSkipAlarm, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_color(_btnSkipAlarm, lv_color_hex(C_SKIP_BORD), 0);
        lv_obj_set_style_border_width(_btnSkipAlarm, 1, 0);
        lv_obj_add_event_cb(_btnSkipAlarm, _skipBtnEventCb, LV_EVENT_CLICKED, this);

        lv_obj_t* skipLbl = lv_label_create(_btnSkipAlarm);
        lv_label_set_text(skipLbl, "Next");
        lv_obj_set_style_text_font(skipLbl, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(skipLbl, lv_color_hex(C_SKIP_TXT), 0);
        lv_obj_center(skipLbl);
    }

    // -----------------------------------------------------------------------
    // Sensor strip  (bottom, full width)
    // -----------------------------------------------------------------------
    lv_obj_t* sensorStrip = lv_obj_create(scr);
    lv_obj_set_pos(sensorStrip, 0, SCREEN_H - SENSOR_H);
    lv_obj_set_size(sensorStrip, SCREEN_W, SENSOR_H);
    lv_obj_set_style_bg_color(sensorStrip, lv_color_hex(C_SENSOR_BG), 0);
    lv_obj_set_style_bg_opa(sensorStrip, LV_OPA_COVER, 0);
    applyContainerStyle(sensorStrip);

    // Top border of sensor strip
    lv_obj_t* sensorDiv = lv_obj_create(scr);
    lv_obj_set_pos(sensorDiv, 0, SCREEN_H - SENSOR_H);
    lv_obj_set_size(sensorDiv, SCREEN_W, 1);
    lv_obj_set_style_bg_color(sensorDiv, lv_color_hex(C_DIVIDER), 0);
    lv_obj_set_style_bg_opa(sensorDiv, LV_OPA_COVER, 0);
    applyContainerStyle(sensorDiv);

    static const char* sNames[]  = { "TEMP", "FEUCHTE", "CO2", "TVOC" };
    static const char* sValues[] = { "--\xc2\xb0\x43", "--%", "---", "---" };
    lv_obj_t** const valueSlots[4] = { &_lblTemp, &_lblHum, &_lblCO2, &_lblTVOC };
    const int COL_W = SCREEN_W / 4;  // 200 px per column

    for (int i = 0; i < 4; i++) {
        lv_obj_t* lblN = lv_label_create(sensorStrip);
        lv_label_set_text(lblN, sNames[i]);
        lv_obj_set_style_text_font(lblN, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(lblN, lv_color_hex(C_SENSOR_LBL), 0);
        lv_obj_set_pos(lblN, i * COL_W, 4);
        lv_obj_set_width(lblN, COL_W);
        lv_obj_set_style_text_align(lblN, LV_TEXT_ALIGN_CENTER, 0);

        lv_obj_t* lblV = lv_label_create(sensorStrip);
        lv_label_set_text(lblV, sValues[i]);
        lv_obj_set_style_text_font(lblV, &lv_font_montserrat_16, 0);
        lv_obj_set_style_text_color(lblV, lv_color_hex(C_SENSOR_LBL), 0);
        lv_obj_set_pos(lblV, i * COL_W, 22);
        lv_obj_set_width(lblV, COL_W);
        lv_obj_set_style_text_align(lblV, LV_TEXT_ALIGN_CENTER, 0);
        *valueSlots[i] = lblV;
    }

    lv_unlock();
}

// ---------------------------------------------------------------------------
void MainScreen::updateTime(const struct tm& t) {
    if (!_lblWeekday || !_lblDate || !_timeDigits[0]) return;

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
        lv_label_set_text(_lblWeekday, _germanDay(t.tm_wday));
        snprintf(buf, sizeof(buf), "%02d. %s %04d",
                 t.tm_mday, _germanMonthShort(t.tm_mon), t.tm_year + 1900);
        lv_label_set_text(_lblDate, buf);
        s_last_mday = t.tm_mday;
    }
    if (time_changed) {
        const char digs[8] = {
            (char)('0' + t.tm_hour / 10), (char)('0' + t.tm_hour % 10), ':',
            (char)('0' + t.tm_min  / 10), (char)('0' + t.tm_min  % 10), ':',
            (char)('0' + t.tm_sec  / 10), (char)('0' + t.tm_sec  % 10)
        };
        char tmp[2] = {0, 0};
        for (int i = 0; i < 8; i++) {
            if (i == 2 || i == 5) continue;  // colons are static
            tmp[0] = digs[i];
            lv_label_set_text(_timeDigits[i], tmp);
        }
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
