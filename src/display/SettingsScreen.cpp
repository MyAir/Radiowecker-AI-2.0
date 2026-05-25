#include "SettingsScreen.h"

// ---------------------------------------------------------------------------
// Colour palette  (light blue/white — mirrors examples/mockups/alarm_setup_1.png)
// ---------------------------------------------------------------------------
static constexpr uint32_t SC_BG         = 0xF1F5F9;  // page bg (slate-100)
static constexpr uint32_t SC_BAR_BG     = 0xF1F5F9;  // title bar matches page
static constexpr uint32_t SC_SECT_TXT   = 0x64748B;  // section labels (slate-500)
static constexpr uint32_t SC_TITLE      = 0x0F172A;  // title text (slate-900)
static constexpr uint32_t SC_DIVIDER    = 0xCBD5E1;  // divider line (slate-300)
static constexpr uint32_t SC_BTN_BORD   = 0xCBD5E1;  // all button borders
static constexpr uint32_t SC_BTN_ACT    = 0x2563EB;  // active button text (blue-600)
static constexpr uint32_t SC_BACK_TXT   = 0x1E293B;  // back button text

static constexpr int SCREEN_W   = 800;
static constexpr int BAR_H      = 55;   // title bar height
static constexpr uint32_t TIMEOUT_MS = 30000;  // 30 s inactivity

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
static void applyNoStyle(lv_obj_t* obj) {
    lv_obj_set_style_border_width(obj, 0, 0);
    lv_obj_set_style_pad_all(obj, 0, 0);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
}

static lv_obj_t* makeDivider(lv_obj_t* parent, int y) {
    lv_obj_t* d = lv_obj_create(parent);
    lv_obj_set_pos(d, 40, y);
    lv_obj_set_size(d, 720, 1);
    lv_obj_set_style_bg_color(d, lv_color_hex(SC_DIVIDER), 0);
    lv_obj_set_style_bg_opa(d, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(d, 0, 0);
    lv_obj_set_style_pad_all(d, 0, 0);
    return d;
}

static lv_obj_t* makeSectionLabel(lv_obj_t* parent, const char* text, int x, int y) {
    lv_obj_t* lbl = lv_label_create(parent);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl, lv_color_hex(SC_SECT_TXT), 0);
    lv_obj_set_pos(lbl, x, y);
    return lbl;
}

// ---------------------------------------------------------------------------
// _makeBtn
// ---------------------------------------------------------------------------
lv_obj_t* SettingsScreen::_makeBtn(lv_obj_t* parent, const char* label,
                                    int x, int y, int w, int h,
                                    uint32_t textColor, bool dimBorder) {
    lv_obj_t* btn = lv_button_create(parent);
    lv_obj_set_pos(btn, x, y);
    lv_obj_set_size(btn, w, h);
    lv_obj_set_style_radius(btn, 6, 0);
    lv_obj_set_style_bg_opa(btn, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_color(btn, lv_color_hex(dimBorder ? 0xE2E8F0 : SC_BTN_BORD), 0);
    lv_obj_set_style_border_width(btn, 1, 0);

    lv_obj_t* lbl = lv_label_create(btn);
    lv_label_set_text(lbl, label);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(lbl, lv_color_hex(textColor), 0);
    lv_obj_center(lbl);
    return btn;
}

// ---------------------------------------------------------------------------
// _goBack()  —  slide back to main screen; auto-deletes settings screen
// ---------------------------------------------------------------------------
void SettingsScreen::_goBack() {
    if (!_scr) return;
    if (_timer) {
        // Delete the timer (safe: only called from button callbacks, not from
        // within the timer's own callback — see _timeoutCb for that path)
        lv_timer_delete(_timer);
        _timer = nullptr;
    }
    // auto_del=true: LVGL deletes the previous screen (_scr) after animation
    lv_screen_load_anim(_mainScr, LV_SCR_LOAD_ANIM_MOVE_LEFT, 300, 0, true);
    _scr = nullptr;  // mark as gone; LVGL owns and will delete it
}

// ---------------------------------------------------------------------------
// create()
// ---------------------------------------------------------------------------
void SettingsScreen::create(lv_obj_t* mainScr) {
    if (_scr) return;  // already open
    _mainScr = mainScr;

    // -----------------------------------------------------------------------
    // Screen root
    // -----------------------------------------------------------------------
    _scr = lv_obj_create(NULL);
    lv_obj_set_size(_scr, SCREEN_W, 480);
    lv_obj_set_style_bg_color(_scr, lv_color_hex(SC_BG), 0);
    lv_obj_set_style_bg_opa(_scr, LV_OPA_COVER, 0);
    applyNoStyle(_scr);

    // -----------------------------------------------------------------------
    // Title bar
    // -----------------------------------------------------------------------
    lv_obj_t* bar = lv_obj_create(_scr);
    lv_obj_set_pos(bar, 0, 0);
    lv_obj_set_size(bar, SCREEN_W, BAR_H);
    lv_obj_set_style_bg_color(bar, lv_color_hex(SC_BAR_BG), 0);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
    applyNoStyle(bar);

    // Back button
    lv_obj_t* btnBack = _makeBtn(bar, "< Back", 12, 10, 90, 34, SC_BACK_TXT);
    lv_obj_add_event_cb(btnBack, _backBtnCb, LV_EVENT_CLICKED, this);

    // Title
    lv_obj_t* lblTitle = lv_label_create(bar);
    lv_label_set_text(lblTitle, "Einstellungen");
    lv_obj_set_style_text_font(lblTitle, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(lblTitle, lv_color_hex(SC_TITLE), 0);
    lv_obj_align(lblTitle, LV_ALIGN_CENTER, 0, 0);

    // Bar bottom divider
    makeDivider(_scr, BAR_H);

    // -----------------------------------------------------------------------
    // Navigation buttons
    // -----------------------------------------------------------------------
    makeSectionLabel(_scr, "KONFIGURATION", 60, 80);

    lv_obj_t* btnGeneral = _makeBtn(_scr, "System",
                                     60, 110, 320, 60, SC_BTN_ACT);
    lv_obj_add_event_cb(btnGeneral, _openGeneralCb, LV_EVENT_CLICKED, this);

    lv_obj_t* btnAlarms  = _makeBtn(_scr, "Alarms",
                                     420, 110, 320, 60, SC_BTN_ACT);
    lv_obj_add_event_cb(btnAlarms, _openAlarmsCb, LV_EVENT_CLICKED, this);

    _timer = lv_timer_create(_timeoutCb, TIMEOUT_MS, this);
    lv_timer_set_repeat_count(_timer, 1);
    lv_timer_set_auto_delete(_timer, true);

    // -----------------------------------------------------------------------
    // Slide in from left (main screen moves right, settings enters from left)
    // -----------------------------------------------------------------------
    lv_screen_load_anim(_scr, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 300, 0, false);
}

// ---------------------------------------------------------------------------
// Callbacks
// ---------------------------------------------------------------------------
void SettingsScreen::_timeoutCb(lv_timer_t* t) {
    auto* self = static_cast<SettingsScreen*>(lv_timer_get_user_data(t));
    if (!self) return;
    // If another screen is currently overlaying us (e.g. AlarmScreen during a
    // ringing alarm), don't yank our screen out from underneath: reschedule
    // and re-check after another TIMEOUT_MS.
    if (self->_scr && lv_screen_active() != self->_scr) {
        self->_timer = lv_timer_create(_timeoutCb, TIMEOUT_MS, self);
        lv_timer_set_repeat_count(self->_timer, 1);
        lv_timer_set_auto_delete(self->_timer, true);
        return;
    }
    self->_timer = nullptr;  // timer auto-deletes after this callback returns
    self->_goBack();
}

void SettingsScreen::_backBtnCb(lv_event_t* e) {
    static uint32_t lastFire = 0;
    const uint32_t now = lv_tick_get();
    if (now - lastFire < 500) return;
    lastFire = now;
    auto* self = static_cast<SettingsScreen*>(lv_event_get_user_data(e));
    self->_goBack();
}

// _detachForChildScreen: stop our timer and forget our screen ref so the
// child screen (AlarmSetup / GeneralSettings) can lv_screen_load_anim with
// auto_del=true and have LVGL delete our screen once the animation completes.
void SettingsScreen::_detachForChildScreen() {
    if (_timer) { lv_timer_delete(_timer); _timer = nullptr; }
    _scr = nullptr;
}

void SettingsScreen::_openAlarmsCb(lv_event_t* e) {
    static uint32_t lastFire = 0;
    const uint32_t now = lv_tick_get();
    if (now - lastFire < 500) return;
    lastFire = now;
    auto* self = static_cast<SettingsScreen*>(lv_event_get_user_data(e));
    if (!self) return;
    Callback cb = self->_onOpenAlarms;
    self->_detachForChildScreen();
    if (cb) cb();
}

void SettingsScreen::_openGeneralCb(lv_event_t* e) {
    static uint32_t lastFire = 0;
    const uint32_t now = lv_tick_get();
    if (now - lastFire < 500) return;
    lastFire = now;
    auto* self = static_cast<SettingsScreen*>(lv_event_get_user_data(e));
    if (!self) return;
    Callback cb = self->_onOpenGeneral;
    self->_detachForChildScreen();
    if (cb) cb();
}
