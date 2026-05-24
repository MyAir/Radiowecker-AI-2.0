#include "AlarmScreen.h"
#include "AppConfig.h"
#include "audio/AudioPlayer.h"
#include <stdio.h>
#include <string.h>

// Single global instance (declared extern in AlarmScreen.h).
AlarmScreen alarmScreen;

// ---------------------------------------------------------------------------
// Style constants (match SettingsScreen / MainScreen amber palette)
// ---------------------------------------------------------------------------
static constexpr uint32_t AS_BG          = 0x000000;
static constexpr uint32_t AS_BAR_BG      = 0x121212;
static constexpr uint32_t AS_TITLE       = 0xC86E0F;
static constexpr uint32_t AS_DIM         = 0x7A4409;
static constexpr uint32_t AS_CARD_BORDER = 0x643708;
static constexpr uint32_t AS_CARD_BG     = 0x1A0E04;
static constexpr uint32_t AS_TEXT        = 0xBE690E;
static constexpr uint32_t AS_CLOCK       = 0xE07A14;
static constexpr uint32_t AS_BTN_STOP_BG = 0x6B0E0E;
static constexpr uint32_t AS_BTN_STOP_TX = 0xFFCFCF;
static constexpr uint32_t AS_BTN_SNZ_BG  = 0x3A1E04;
static constexpr uint32_t AS_BTN_SNZ_TX  = 0xE0A050;

static constexpr int SCREEN_W = 800;
static constexpr int SCREEN_H = 480;

// Fonts that contain umlauts
extern "C" const lv_font_t ui_font_ms14m;
extern "C" const lv_font_t ui_font_ms24m;
extern "C" const lv_font_t ui_font_ms36m;
extern "C" const lv_font_t ui_font_ms80m;
extern "C" const lv_font_t ui_font_ms120m;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
static const char* germanWdayLong(int wday) {
    static const char* const d[7] = {
        "Sonntag", "Montag", "Dienstag", "Mittwoch",
        "Donnerstag", "Freitag", "Samstag"
    };
    return (wday >= 0 && wday <= 6) ? d[wday] : "";
}

static lv_obj_t* makeLabel(lv_obj_t* parent, const char* txt, const lv_font_t* font,
                           uint32_t color, int x, int y) {
    lv_obj_t* l = lv_label_create(parent);
    lv_label_set_text(l, txt);
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, lv_color_hex(color), 0);
    lv_obj_set_pos(l, x, y);
    return l;
}

static lv_obj_t* makeCard(lv_obj_t* parent, int x, int y, int w, int h) {
    lv_obj_t* c = lv_obj_create(parent);
    lv_obj_set_pos(c, x, y);
    lv_obj_set_size(c, w, h);
    lv_obj_set_style_bg_color(c, lv_color_hex(AS_CARD_BG), 0);
    lv_obj_set_style_bg_opa(c, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(c, lv_color_hex(AS_CARD_BORDER), 0);
    lv_obj_set_style_border_width(c, 1, 0);
    lv_obj_set_style_radius(c, 8, 0);
    lv_obj_set_style_pad_all(c, 8, 0);
    lv_obj_clear_flag(c, LV_OBJ_FLAG_SCROLLABLE);
    return c;
}

static lv_obj_t* makeBigBtn(lv_obj_t* parent, const char* label, int x, int y,
                            int w, int h, uint32_t bg, uint32_t tx) {
    lv_obj_t* btn = lv_button_create(parent);
    lv_obj_set_pos(btn, x, y);
    lv_obj_set_size(btn, w, h);
    lv_obj_set_style_bg_color(btn, lv_color_hex(bg), 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(btn, lv_color_hex(AS_CARD_BORDER), 0);
    lv_obj_set_style_border_width(btn, 1, 0);
    lv_obj_set_style_radius(btn, 10, 0);

    lv_obj_t* l = lv_label_create(btn);
    lv_label_set_text(l, label);
    lv_obj_set_style_text_font(l, &ui_font_ms24m, 0);
    lv_obj_set_style_text_color(l, lv_color_hex(tx), 0);
    lv_obj_center(l);
    return btn;
}

static void setSlotText(lv_obj_t* tempLbl, lv_obj_t* popLbl,
                        const WeatherManager::Slot& s) {
    char buf[40];
    if (s.valid) {
        snprintf(buf, sizeof(buf), "%.0f\xc2\xb0""C", (double)s.temp);
    } else {
        snprintf(buf, sizeof(buf), "--");
    }
    lv_label_set_text(tempLbl, buf);

    if (popLbl) {
        if (s.valid && s.pop >= 0) snprintf(buf, sizeof(buf), "%d %%", s.pop);
        else                       snprintf(buf, sizeof(buf), "--");
        lv_label_set_text(popLbl, buf);
    }
}

// ---------------------------------------------------------------------------
// show()
// ---------------------------------------------------------------------------
void AlarmScreen::show(lv_obj_t* mainScr, const Alarm& a, uint8_t currentBrightness) {
    if (_scr) return;
    _mainScr           = mainScr;
    _alarm             = a;
    _savedBrightness   = currentBrightness;
    _lastMetaVersion   = 0;
    _lastMinute        = -1;
    _lastWeatherStamp  = -1;

    // Boost panel to full brightness
    if (_onBrightness) _onBrightness(255);

    _scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(_scr, lv_color_hex(AS_BG), 0);
    lv_obj_set_style_bg_opa(_scr, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(_scr, 0, 0);
    lv_obj_clear_flag(_scr, LV_OBJ_FLAG_SCROLLABLE);

    // ---- Header bar ----
    lv_obj_t* bar = lv_obj_create(_scr);
    lv_obj_set_pos(bar, 0, 0);
    lv_obj_set_size(bar, SCREEN_W, 50);
    lv_obj_set_style_bg_color(bar, lv_color_hex(AS_BAR_BG), 0);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(bar, 0, 0);
    lv_obj_set_style_pad_all(bar, 0, 0);
    lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);

    const char* tt = (a.title.length() > 0) ? a.title.c_str() : "Wecker";
    _lblTitle = makeLabel(bar, tt, &ui_font_ms24m, AS_TITLE, 16, 11);

    _lblDate    = makeLabel(bar, "",  &ui_font_ms14m, AS_DIM, SCREEN_W - 230, 8);
    _lblTimeHdr = makeLabel(bar, "",  &ui_font_ms24m, AS_TITLE, SCREEN_W - 80, 11);

    // ---- Hero "Jetzt" card (left) ----
    lv_obj_t* hero = makeCard(_scr, 12, 62, 380, 200);
    makeLabel(hero, "Jetzt", &ui_font_ms24m, AS_DIM, 4, 0);
    _lblHeroTemp = makeLabel(hero, "--\xc2\xb0", &ui_font_ms120m, AS_CLOCK, 6, 38);
    _lblHeroDesc = makeLabel(hero, "",            &ui_font_ms24m,  AS_TEXT,  6, 150);

    // ---- Big clock (right) ----
    lv_obj_t* clockCard = makeCard(_scr, 400, 62, SCREEN_W - 412, 200);
    _lblBigClock = makeLabel(clockCard, "--:--", &ui_font_ms120m, AS_CLOCK, 30, 22);
    _lblMeta     = makeLabel(clockCard, "",      &ui_font_ms14m,  AS_DIM,    6, 150);
    lv_obj_set_width(_lblMeta, SCREEN_W - 412 - 16);
    lv_label_set_long_mode(_lblMeta, LV_LABEL_LONG_DOT);

    // ---- Forecast tile row ----
    auto makeTile = [this](const char* head, int x) -> Tile {
        lv_obj_t* card = makeCard(_scr, x, 274, 252, 120);
        Tile t;
        t.head = makeLabel(card, head, &ui_font_ms24m, AS_TITLE, 4, 0);
        t.temp = makeLabel(card, "--", &ui_font_ms36m, AS_CLOCK, 4, 36);
        t.pop  = makeLabel(card, "--", &ui_font_ms14m, AS_DIM,   4, 80);
        return t;
    };
    _tMorn = makeTile("Morgen fr" "\xc3\xbc" "h", 12);
    _tAft  = makeTile("Nachmittag",                  274);
    _tTom  = makeTile("Morgen",                      536);

    // ---- Action buttons ----
    _btnSnooze = makeBigBtn(_scr, "Schlummern", 12,  410, 380, 60,
                            AS_BTN_SNZ_BG, AS_BTN_SNZ_TX);
    lv_obj_add_event_cb(_btnSnooze, _snoozeBtnCb, LV_EVENT_CLICKED, this);

    _btnStop = makeBigBtn(_scr, "Stop", 400, 410, SCREEN_W - 412, 60,
                          AS_BTN_STOP_BG, AS_BTN_STOP_TX);
    lv_obj_add_event_cb(_btnStop, _stopBtnCb, LV_EVENT_CLICKED, this);

    // Load with auto_del=true so MainScreen would be deleted; but MainScreen
    // is owned by the global instance and must NOT be freed. So use false.
    lv_screen_load_anim(_scr, LV_SCR_LOAD_ANIM_OVER_TOP, 300, 0, false);
}

// ---------------------------------------------------------------------------
// hide()
// ---------------------------------------------------------------------------
void AlarmScreen::hide() {
    if (!_scr) return;
    if (_snoozeTimer) { lv_timer_delete(_snoozeTimer); _snoozeTimer = nullptr; }
    if (_onBrightness) _onBrightness(_savedBrightness);
    // Slide back to main and delete this screen
    lv_screen_load_anim(_mainScr, LV_SCR_LOAD_ANIM_FADE_OUT, 300, 0, true);
    _scr = nullptr;
}

// ---------------------------------------------------------------------------
// tick()  — called once per second from main loop
// ---------------------------------------------------------------------------
void AlarmScreen::tick(const tm& now, const WeatherManager& w,
                       const AudioPlayer& audio) {
    if (!_scr) return;

    // --- Header date + time (right-aligned) ---
    if (now.tm_min != _lastMinute) {
        _lastMinute = now.tm_min;
        char buf[40];
        snprintf(buf, sizeof(buf), "%s %02d.%02d.%04d",
                 germanWdayLong(now.tm_wday),
                 now.tm_mday, now.tm_mon + 1, now.tm_year + 1900);
        lv_label_set_text(_lblDate, buf);

        snprintf(buf, sizeof(buf), "%02d:%02d", now.tm_hour, now.tm_min);
        lv_label_set_text(_lblTimeHdr, buf);
        // Align right
        lv_obj_update_layout(_lblTimeHdr);
        int tw = lv_obj_get_width(_lblTimeHdr);
        lv_obj_set_pos(_lblTimeHdr, SCREEN_W - tw - 16, 11);
        lv_obj_update_layout(_lblDate);
        int dw = lv_obj_get_width(_lblDate);
        lv_obj_set_pos(_lblDate, SCREEN_W - dw - 16 - tw - 16, 18);

        lv_label_set_text(_lblBigClock, buf);
    }

    // --- Weather ---
    int stamp = w.hasData() ? (int)((w.current().valid ? 1 : 0)
                                    + (int)w.current().temp * 10) : -1;
    if (stamp != _lastWeatherStamp) {
        _lastWeatherStamp = stamp;
        const auto& cur = w.current();
        char buf[60];
        if (cur.valid) {
            snprintf(buf, sizeof(buf), "%.0f\xc2\xb0", (double)cur.temp);
            lv_label_set_text(_lblHeroTemp, buf);
            lv_label_set_text(_lblHeroDesc, cur.desc);
        } else {
            lv_label_set_text(_lblHeroTemp, "--\xc2\xb0");
            lv_label_set_text(_lblHeroDesc, "Keine Daten");
        }
        setSlotText(_tMorn.temp, _tMorn.pop, w.morning());
        setSlotText(_tAft.temp,  _tAft.pop,  w.afternoon());
        setSlotText(_tTom.temp,  _tTom.pop,  w.tomorrow());
    }

    // --- Now-playing metadata ---
    if (audio.metadataVersion() != _lastMetaVersion) {
        _lastMetaVersion = audio.metadataVersion();
        char buf[AUDIO_META_MAX];
        audio.metadata(buf, sizeof(buf));
        lv_label_set_text(_lblMeta, buf);
    }
}

// ---------------------------------------------------------------------------
// Button callbacks
// ---------------------------------------------------------------------------
void AlarmScreen::_snoozeBtnCb(lv_event_t* e) {
    auto* self = static_cast<AlarmScreen*>(lv_event_get_user_data(e));
    if (!self || !self->_scr) return;

    if (self->_onStop) self->_onStop();

    // Schedule a one-shot re-fire after snoozeMinutes()
    if (self->_snoozeTimer) {
        lv_timer_delete(self->_snoozeTimer);
        self->_snoozeTimer = nullptr;
    }
    uint32_t snoozeMs = (uint32_t)g_appConfig.snoozeMinutes() * 60u * 1000u;
    self->_snoozeTimer = lv_timer_create(_snoozeTimerCb, snoozeMs, self);
    lv_timer_set_repeat_count(self->_snoozeTimer, 1);

    self->hide();
}

void AlarmScreen::_stopBtnCb(lv_event_t* e) {
    auto* self = static_cast<AlarmScreen*>(lv_event_get_user_data(e));
    if (!self || !self->_scr) return;
    if (self->_onStop) self->_onStop();
    self->hide();
}

void AlarmScreen::_snoozeTimerCb(lv_timer_t* t) {
    auto* self = static_cast<AlarmScreen*>(lv_timer_get_user_data(t));
    if (!self) return;
    self->_snoozeTimer = nullptr;  // one-shot consumed
    if (self->_onSnoozeFire) self->_onSnoozeFire(self->_alarm);
}
