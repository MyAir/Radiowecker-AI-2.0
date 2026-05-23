#include "SettingsScreen.h"

// ---------------------------------------------------------------------------
// Colour palette  (same amber theme as MainScreen)
// ---------------------------------------------------------------------------
static constexpr uint32_t SC_BG         = 0x000000;
static constexpr uint32_t SC_BAR_BG     = 0x121212;
static constexpr uint32_t SC_SECT_TXT   = 0x646464;  // section labels
static constexpr uint32_t SC_TITLE      = 0xA05A0C;  // title text
static constexpr uint32_t SC_DIVIDER    = 0x502D05;
static constexpr uint32_t SC_BTN_BORD   = 0x643708;  // all button borders
static constexpr uint32_t SC_BTN_DIM    = 0x4A2A06;  // mock button text (inactive)
static constexpr uint32_t SC_BTN_ACT    = 0xBE690E;  // working button text
static constexpr uint32_t SC_BACK_TXT   = 0x7A4409;  // back button text
static constexpr uint32_t SC_SLD_TRACK  = 0x1A0E04;  // slider track bg
static constexpr uint32_t SC_SLD_FILL   = 0xA05A0C;  // slider filled part
static constexpr uint32_t SC_SLD_KNOB   = 0xC86E0F;  // slider knob

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
    lv_obj_set_style_border_color(btn, lv_color_hex(dimBorder ? 0x3A1E04 : SC_BTN_BORD), 0);
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
void SettingsScreen::create(lv_obj_t* mainScr, uint8_t currentVolume, uint8_t currentBrightness) {
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
    // Section 1 — mock-up navigation buttons
    // -----------------------------------------------------------------------
    makeSectionLabel(_scr, "KONFIGURATION", 60, 68);

    lv_obj_t* btnGeneral = _makeBtn(_scr, "General Settings",
                                     60, 88, 280, 50, SC_BTN_DIM, true);
    lv_obj_add_event_cb(btnGeneral, _mockBtnCb, LV_EVENT_CLICKED, this);

    lv_obj_t* btnAlarms  = _makeBtn(_scr, "Alarms",
                                     370, 88, 280, 50, SC_BTN_DIM, true);
    lv_obj_add_event_cb(btnAlarms, _mockBtnCb, LV_EVENT_CLICKED, this);

    makeDivider(_scr, 152);

    // -----------------------------------------------------------------------
    // Section 2 — working audio controls
    // -----------------------------------------------------------------------
    makeSectionLabel(_scr, "AUDIO", 60, 160);

    lv_obj_t* btnPlaySD = _makeBtn(_scr, LV_SYMBOL_PLAY " SD MP3",
                                    60, 180, 205, 50, SC_BTN_ACT);
    lv_obj_add_event_cb(btnPlaySD, _playSDCb, LV_EVENT_CLICKED, this);

    lv_obj_t* btnSRF3   = _makeBtn(_scr, LV_SYMBOL_PLAY " SRF 3",
                                    290, 180, 205, 50, SC_BTN_ACT);
    lv_obj_add_event_cb(btnSRF3, _playSRF3Cb, LV_EVENT_CLICKED, this);

    lv_obj_t* btnStop   = _makeBtn(_scr, LV_SYMBOL_STOP " Stop",
                                    520, 180, 205, 50, SC_BTN_ACT);
    lv_obj_add_event_cb(btnStop, _stopCb, LV_EVENT_CLICKED, this);

    makeDivider(_scr, 244);

    // -----------------------------------------------------------------------
    // Section 3 — volume slider
    // -----------------------------------------------------------------------
    makeSectionLabel(_scr, "VOLUME", 60, 252);

    _slider = lv_slider_create(_scr);
    lv_obj_set_pos(_slider, 60, 278);
    lv_obj_set_size(_slider, 680, 32);
    lv_slider_set_range(_slider, 0, 21);
    lv_slider_set_value(_slider, currentVolume, LV_ANIM_OFF);

    // Track
    lv_obj_set_style_bg_color(_slider, lv_color_hex(SC_SLD_TRACK), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(_slider, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_color(_slider, lv_color_hex(SC_BTN_BORD), LV_PART_MAIN);
    lv_obj_set_style_border_width(_slider, 1, LV_PART_MAIN);
    lv_obj_set_style_radius(_slider, 4, LV_PART_MAIN);
    lv_obj_set_style_pad_ver(_slider, 0, LV_PART_MAIN);

    // Filled indicator
    lv_obj_set_style_bg_color(_slider, lv_color_hex(SC_SLD_FILL), LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(_slider, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_radius(_slider, 4, LV_PART_INDICATOR);

    // Knob
    lv_obj_set_style_bg_color(_slider, lv_color_hex(SC_SLD_KNOB), LV_PART_KNOB);
    lv_obj_set_style_bg_opa(_slider, LV_OPA_COVER, LV_PART_KNOB);
    lv_obj_set_style_radius(_slider, 5, LV_PART_KNOB);
    lv_obj_set_style_pad_all(_slider, 6, LV_PART_KNOB);

    lv_obj_add_event_cb(_slider, _volumeCb, LV_EVENT_VALUE_CHANGED, this);

    makeDivider(_scr, 322);

    // -----------------------------------------------------------------------
    // Section 4 — brightness slider
    // -----------------------------------------------------------------------
    makeSectionLabel(_scr, "HELLIGKEIT", 60, 330);

    _brightnessSlider = lv_slider_create(_scr);
    lv_obj_set_pos(_brightnessSlider, 60, 356);
    lv_obj_set_size(_brightnessSlider, 680, 32);
    lv_slider_set_range(_brightnessSlider, 10, 255);
    lv_slider_set_value(_brightnessSlider, currentBrightness, LV_ANIM_OFF);

    lv_obj_set_style_bg_color(_brightnessSlider, lv_color_hex(SC_SLD_TRACK), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(_brightnessSlider, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_color(_brightnessSlider, lv_color_hex(SC_BTN_BORD), LV_PART_MAIN);
    lv_obj_set_style_border_width(_brightnessSlider, 1, LV_PART_MAIN);
    lv_obj_set_style_radius(_brightnessSlider, 4, LV_PART_MAIN);
    lv_obj_set_style_pad_ver(_brightnessSlider, 0, LV_PART_MAIN);

    lv_obj_set_style_bg_color(_brightnessSlider, lv_color_hex(SC_SLD_FILL), LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(_brightnessSlider, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_radius(_brightnessSlider, 4, LV_PART_INDICATOR);

    lv_obj_set_style_bg_color(_brightnessSlider, lv_color_hex(SC_SLD_KNOB), LV_PART_KNOB);
    lv_obj_set_style_bg_opa(_brightnessSlider, LV_OPA_COVER, LV_PART_KNOB);
    lv_obj_set_style_radius(_brightnessSlider, 5, LV_PART_KNOB);
    lv_obj_set_style_pad_all(_brightnessSlider, 6, LV_PART_KNOB);

    lv_obj_add_event_cb(_brightnessSlider, _brightnessCb, LV_EVENT_VALUE_CHANGED, this);
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

void SettingsScreen::_mockBtnCb(lv_event_t* e) {
    // No action — just resets the inactivity timer
    auto* self = static_cast<SettingsScreen*>(lv_event_get_user_data(e));
    if (self && self->_timer) lv_timer_reset(self->_timer);
}

void SettingsScreen::_playSDCb(lv_event_t* e) {
    static uint32_t lastFire = 0;
    const uint32_t now = lv_tick_get();
    if (now - lastFire < 500) return;
    lastFire = now;
    auto* self = static_cast<SettingsScreen*>(lv_event_get_user_data(e));
    if (!self) return;
    if (self->_timer) lv_timer_reset(self->_timer);
    if (self->_onPlaySD) self->_onPlaySD();
}

void SettingsScreen::_playSRF3Cb(lv_event_t* e) {
    static uint32_t lastFire = 0;
    const uint32_t now = lv_tick_get();
    if (now - lastFire < 500) return;
    lastFire = now;
    auto* self = static_cast<SettingsScreen*>(lv_event_get_user_data(e));
    if (!self) return;
    if (self->_timer) lv_timer_reset(self->_timer);
    if (self->_onPlaySRF3) self->_onPlaySRF3();
}

void SettingsScreen::_stopCb(lv_event_t* e) {
    static uint32_t lastFire = 0;
    const uint32_t now = lv_tick_get();
    if (now - lastFire < 500) return;
    lastFire = now;
    auto* self = static_cast<SettingsScreen*>(lv_event_get_user_data(e));
    if (!self) return;
    if (self->_timer) lv_timer_reset(self->_timer);
    if (self->_onStop) self->_onStop();
}

void SettingsScreen::_volumeCb(lv_event_t* e) {
    auto* self = static_cast<SettingsScreen*>(lv_event_get_user_data(e));
    if (!self || !self->_slider) return;
    if (self->_timer) lv_timer_reset(self->_timer);
    if (self->_onVolume) {
        uint8_t vol = (uint8_t)lv_slider_get_value(self->_slider);
        self->_onVolume(vol);
    }
}

void SettingsScreen::_brightnessCb(lv_event_t* e) {
    auto* self = static_cast<SettingsScreen*>(lv_event_get_user_data(e));
    if (!self || !self->_brightnessSlider) return;
    if (self->_timer) lv_timer_reset(self->_timer);
    if (self->_onBrightness) {
        uint8_t br = (uint8_t)lv_slider_get_value(self->_brightnessSlider);
        self->_onBrightness(br);
    }
}
