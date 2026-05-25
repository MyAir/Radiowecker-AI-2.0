#include "GeneralSettingsScreen.h"
#include "AppConfig.h"

GeneralSettingsScreen generalSettingsScreen;

// Palette (light blue/white — mirrors examples/mockups/alarm_setup_1.png)
static constexpr uint32_t GS_BG       = 0xF1F5F9;  // page bg (slate-100)
static constexpr uint32_t GS_BAR_BG   = 0xF1F5F9;  // title bar matches page
static constexpr uint32_t GS_TITLE    = 0x0F172A;  // title text (slate-900)
static constexpr uint32_t GS_BTN_BORD = 0xCBD5E1;  // button borders
static constexpr uint32_t GS_BACK_TXT = 0x1E293B;  // back button text
static constexpr uint32_t GS_TEXT     = 0x1E293B;  // body text (slate-800)
static constexpr uint32_t GS_DIM      = 0x64748B;  // muted text (slate-500)
static constexpr uint32_t GS_SLD_TRACK = 0xE2E8F0; // slider track (slate-200)
static constexpr uint32_t GS_SLD_FILL  = 0x2563EB; // slider fill (blue-600)
static constexpr uint32_t GS_SLD_KNOB  = 0x2563EB; // slider knob

extern "C" const lv_font_t ui_font_ms14m;
extern "C" const lv_font_t ui_font_ms24m;
extern "C" const lv_font_t ui_font_ms36m;
extern "C" const lv_font_t ui_font_ms80m;

static constexpr int SCREEN_W = 800;
static constexpr int BAR_H    = 55;

// ---------------------------------------------------------------------------
// create()
// ---------------------------------------------------------------------------
void GeneralSettingsScreen::create(lv_obj_t* mainScr, uint8_t currentBrightness,
                                   const char* alarmOptions) {
    // If we still hold a screen handle from a previous open (e.g. the user
    // triggered a test alarm which overlaid AlarmScreen and then jumped back
    // to MainScreen, orphaning this screen), tear it down so we can re-open
    // cleanly. Without this the System/Alarms buttons appear dead until the
    // orphaned timeout timer fires _goBack and finally clears _scr.
    if (_scr) {
        if (_timer) { lv_timer_delete(_timer); _timer = nullptr; }
        lv_obj_delete(_scr);
        _scr = nullptr;
    }
    _mainScr = mainScr;

    _scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(_scr, lv_color_hex(GS_BG), 0);
    lv_obj_set_style_bg_opa(_scr, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(_scr, 0, 0);
    lv_obj_clear_flag(_scr, LV_OBJ_FLAG_SCROLLABLE);

    // ---- Title bar ----
    lv_obj_t* bar = lv_obj_create(_scr);
    lv_obj_set_pos(bar, 0, 0);
    lv_obj_set_size(bar, SCREEN_W, BAR_H);
    lv_obj_set_style_bg_color(bar, lv_color_hex(GS_BAR_BG), 0);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(bar, 0, 0);
    lv_obj_set_style_pad_all(bar, 0, 0);
    lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* btnBack = lv_button_create(bar);
    lv_obj_set_pos(btnBack, 12, 10);
    lv_obj_set_size(btnBack, 100, 36);
    lv_obj_set_style_bg_opa(btnBack, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_color(btnBack, lv_color_hex(GS_BTN_BORD), 0);
    lv_obj_set_style_border_width(btnBack, 1, 0);
    lv_obj_set_style_radius(btnBack, 6, 0);
    lv_obj_t* lblBack = lv_label_create(btnBack);
    // "Zurück" with umlaut
    lv_label_set_text(lblBack, "< Zur\xc3\xbcck");
    lv_obj_set_style_text_font(lblBack, &ui_font_ms14m, 0);
    lv_obj_set_style_text_color(lblBack, lv_color_hex(GS_BACK_TXT), 0);
    lv_obj_center(lblBack);
    lv_obj_add_event_cb(btnBack, _backBtnCb, LV_EVENT_CLICKED, this);

    lv_obj_t* lblTitle = lv_label_create(bar);
    lv_label_set_text(lblTitle, "Allgemeine Einstellungen");
    lv_obj_set_style_text_font(lblTitle, &ui_font_ms24m, 0);
    lv_obj_set_style_text_color(lblTitle, lv_color_hex(GS_TITLE), 0);
    lv_obj_align(lblTitle, LV_ALIGN_CENTER, 0, 0);

    // ---- Snooze duration row ----
    lv_obj_t* lblSnooze = lv_label_create(_scr);
    // "Schlummerdauer (Min.)"
    lv_label_set_text(lblSnooze, "Schlummerdauer (Min.)");
    lv_obj_set_style_text_font(lblSnooze, &ui_font_ms24m, 0);
    lv_obj_set_style_text_color(lblSnooze, lv_color_hex(GS_TEXT), 0);
    lv_obj_set_pos(lblSnooze, 40, 120);

    // Decrement button
    lv_obj_t* btnDec = lv_button_create(_scr);
    lv_obj_set_pos(btnDec, 420, 110);
    lv_obj_set_size(btnDec, 60, 60);
    lv_obj_set_style_bg_opa(btnDec, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_color(btnDec, lv_color_hex(GS_BTN_BORD), 0);
    lv_obj_set_style_border_width(btnDec, 1, 0);
    lv_obj_set_style_radius(btnDec, 8, 0);
    lv_obj_t* lblDec = lv_label_create(btnDec);
    lv_label_set_text(lblDec, "-");
    lv_obj_set_style_text_font(lblDec, &ui_font_ms36m, 0);
    lv_obj_set_style_text_color(lblDec, lv_color_hex(GS_TEXT), 0);
    lv_obj_center(lblDec);
    lv_obj_add_event_cb(btnDec, _spinDecCb, LV_EVENT_CLICKED, this);

    // Value spinbox
    _spinSnooze = lv_spinbox_create(_scr);
    lv_spinbox_set_range(_spinSnooze, 1, 30);
    lv_spinbox_set_digit_format(_spinSnooze, 2, 0);
    lv_spinbox_set_step(_spinSnooze, 1);
    lv_spinbox_set_value(_spinSnooze, g_appConfig.snoozeMinutes());
    lv_obj_set_pos(_spinSnooze, 500, 110);
    lv_obj_set_size(_spinSnooze, 120, 60);
    lv_obj_set_style_text_font(_spinSnooze, &ui_font_ms36m, 0);
    lv_obj_set_style_text_color(_spinSnooze, lv_color_hex(GS_TEXT), 0);
    lv_obj_set_style_text_align(_spinSnooze, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_bg_opa(_spinSnooze, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_color(_spinSnooze, lv_color_hex(GS_BTN_BORD), 0);
    lv_obj_set_style_border_width(_spinSnooze, 1, 0);
    lv_obj_set_style_radius(_spinSnooze, 8, 0);
    lv_obj_add_event_cb(_spinSnooze, _spinValueCb, LV_EVENT_VALUE_CHANGED, this);

    // Increment button
    lv_obj_t* btnInc = lv_button_create(_scr);
    lv_obj_set_pos(btnInc, 640, 110);
    lv_obj_set_size(btnInc, 60, 60);
    lv_obj_set_style_bg_opa(btnInc, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_color(btnInc, lv_color_hex(GS_BTN_BORD), 0);
    lv_obj_set_style_border_width(btnInc, 1, 0);
    lv_obj_set_style_radius(btnInc, 8, 0);
    lv_obj_t* lblInc = lv_label_create(btnInc);
    lv_label_set_text(lblInc, "+");
    lv_obj_set_style_text_font(lblInc, &ui_font_ms36m, 0);
    lv_obj_set_style_text_color(lblInc, lv_color_hex(GS_TEXT), 0);
    lv_obj_center(lblInc);
    lv_obj_add_event_cb(btnInc, _spinIncCb, LV_EVENT_CLICKED, this);

    // Hint
    lv_obj_t* hint = lv_label_create(_scr);
    lv_label_set_text(hint, "Dauer zwischen \"Schlummern\" und erneutem Wecken.");
    lv_obj_set_style_text_font(hint, &ui_font_ms14m, 0);
    lv_obj_set_style_text_color(hint, lv_color_hex(GS_DIM), 0);
    lv_obj_set_pos(hint, 40, 200);

    // ---- Brightness row ----
    lv_obj_t* lblBri = lv_label_create(_scr);
    lv_label_set_text(lblBri, "Helligkeit");
    lv_obj_set_style_text_font(lblBri, &ui_font_ms24m, 0);
    lv_obj_set_style_text_color(lblBri, lv_color_hex(GS_TEXT), 0);
    lv_obj_set_pos(lblBri, 40, 280);

    _brightnessSlider = lv_slider_create(_scr);
    lv_obj_set_pos(_brightnessSlider, 40, 330);
    lv_obj_set_size(_brightnessSlider, 720, 32);
    lv_slider_set_range(_brightnessSlider, 10, 255);
    lv_slider_set_value(_brightnessSlider, currentBrightness, LV_ANIM_OFF);

    lv_obj_set_style_bg_color(_brightnessSlider, lv_color_hex(GS_SLD_TRACK), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(_brightnessSlider, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_color(_brightnessSlider, lv_color_hex(GS_BTN_BORD), LV_PART_MAIN);
    lv_obj_set_style_border_width(_brightnessSlider, 1, LV_PART_MAIN);
    lv_obj_set_style_radius(_brightnessSlider, 4, LV_PART_MAIN);
    lv_obj_set_style_pad_ver(_brightnessSlider, 0, LV_PART_MAIN);

    lv_obj_set_style_bg_color(_brightnessSlider, lv_color_hex(GS_SLD_FILL), LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(_brightnessSlider, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_radius(_brightnessSlider, 4, LV_PART_INDICATOR);

    lv_obj_set_style_bg_color(_brightnessSlider, lv_color_hex(GS_SLD_KNOB), LV_PART_KNOB);
    lv_obj_set_style_bg_opa(_brightnessSlider, LV_OPA_COVER, LV_PART_KNOB);
    lv_obj_set_style_radius(_brightnessSlider, 5, LV_PART_KNOB);
    lv_obj_set_style_pad_all(_brightnessSlider, 6, LV_PART_KNOB);

    lv_obj_add_event_cb(_brightnessSlider, _brightnessCb, LV_EVENT_VALUE_CHANGED, this);

    // ---- Debug: fire alarm ----
    if (alarmOptions && alarmOptions[0] != '\0') {
        // Section divider
        lv_obj_t* dbgDiv = lv_obj_create(_scr);
        lv_obj_set_pos(dbgDiv, 40, 380);
        lv_obj_set_size(dbgDiv, 720, 1);
        lv_obj_set_style_bg_color(dbgDiv, lv_color_hex(GS_BTN_BORD), 0);
        lv_obj_set_style_bg_opa(dbgDiv, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(dbgDiv, 0, 0);
        lv_obj_set_style_pad_all(dbgDiv, 0, 0);
        lv_obj_set_style_radius(dbgDiv, 0, 0);
        lv_obj_clear_flag(dbgDiv, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t* lblDbg = lv_label_create(_scr);
        lv_label_set_text(lblDbg, "Debug: Wecker ausl\xc3\xb6sen");
        lv_obj_set_style_text_font(lblDbg, &ui_font_ms14m, 0);
        lv_obj_set_style_text_color(lblDbg, lv_color_hex(GS_DIM), 0);
        lv_obj_set_pos(lblDbg, 40, 390);

        // Alarm dropdown
        _alarmDropdown = lv_dropdown_create(_scr);
        lv_dropdown_set_options(_alarmDropdown, alarmOptions);
        lv_obj_set_pos(_alarmDropdown, 40, 416);
        lv_obj_set_size(_alarmDropdown, 510, 44);
        lv_obj_set_style_bg_color(_alarmDropdown, lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_bg_opa(_alarmDropdown, LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(_alarmDropdown, lv_color_hex(GS_BTN_BORD), 0);
        lv_obj_set_style_border_width(_alarmDropdown, 1, 0);
        lv_obj_set_style_radius(_alarmDropdown, 6, 0);
        lv_obj_set_style_text_font(_alarmDropdown, &ui_font_ms14m, 0);
        lv_obj_set_style_text_color(_alarmDropdown, lv_color_hex(GS_TEXT), 0);
        lv_obj_set_style_pad_left(_alarmDropdown, 10, 0);
        lv_dropdown_set_dir(_alarmDropdown, LV_DIR_TOP);
        lv_dropdown_set_symbol(_alarmDropdown, NULL);
        lv_obj_t* dlist = lv_dropdown_get_list(_alarmDropdown);
        lv_obj_set_style_bg_color(dlist, lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_bg_opa(dlist, LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(dlist, lv_color_hex(GS_BTN_BORD), 0);
        lv_obj_set_style_border_width(dlist, 1, 0);
        lv_obj_set_style_text_font(dlist, &ui_font_ms14m, 0);
        lv_obj_set_style_text_color(dlist, lv_color_hex(GS_TEXT), 0);
        lv_obj_set_style_max_height(dlist, 200, 0);
        lv_obj_add_event_cb(_alarmDropdown,
            [](lv_event_t* ev) {
                auto* s = static_cast<GeneralSettingsScreen*>(lv_event_get_user_data(ev));
                if (s) s->_resetTimer();
            },
            LV_EVENT_VALUE_CHANGED, this);

        // Test button
        lv_obj_t* btnTest = lv_button_create(_scr);
        lv_obj_set_pos(btnTest, 570, 416);
        lv_obj_set_size(btnTest, 190, 44);
        lv_obj_set_style_bg_color(btnTest, lv_color_hex(0x2563EB), 0);
        lv_obj_set_style_bg_opa(btnTest, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(btnTest, 0, 0);
        lv_obj_set_style_radius(btnTest, 6, 0);
        lv_obj_t* lblTest = lv_label_create(btnTest);
        lv_label_set_text(lblTest, "Testen");
        lv_obj_set_style_text_font(lblTest, &ui_font_ms24m, 0);
        lv_obj_set_style_text_color(lblTest, lv_color_hex(0xFFFFFF), 0);
        lv_obj_center(lblTest);
        lv_obj_add_event_cb(btnTest, _testBtnCb, LV_EVENT_CLICKED, this);
    }

    // Inactivity timer
    _timer = lv_timer_create(_timeoutCb, TIMEOUT_MS, this);

    // Slide in from below (settings was below us) — auto_del=true deletes
    // the previously active settings screen (its _scr was detached but
    // LVGL still owns the lv_obj_t and frees it after the animation).
    lv_screen_load_anim(_scr, LV_SCR_LOAD_ANIM_MOVE_LEFT, 300, 0, true);
}

// ---------------------------------------------------------------------------
// _goBack
// ---------------------------------------------------------------------------
void GeneralSettingsScreen::_goBack() {
    if (!_scr) return;
    if (_timer) { lv_timer_delete(_timer); _timer = nullptr; }
    lv_screen_load_anim(_mainScr, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 300, 0, true);
    _scr = nullptr;
}

void GeneralSettingsScreen::_resetTimer() {
    if (_timer) lv_timer_reset(_timer);
}

// ---------------------------------------------------------------------------
// Event callbacks
// ---------------------------------------------------------------------------
void GeneralSettingsScreen::_backBtnCb(lv_event_t* e) {
    static uint32_t lastFire = 0;
    const uint32_t now = lv_tick_get();
    if (now - lastFire < 500) return;
    lastFire = now;
    auto* self = static_cast<GeneralSettingsScreen*>(lv_event_get_user_data(e));
    if (self) self->_goBack();
}

void GeneralSettingsScreen::_spinDecCb(lv_event_t* e) {
    auto* self = static_cast<GeneralSettingsScreen*>(lv_event_get_user_data(e));
    if (!self || !self->_spinSnooze) return;
    lv_spinbox_decrement(self->_spinSnooze);
    self->_resetTimer();
}

void GeneralSettingsScreen::_spinIncCb(lv_event_t* e) {
    auto* self = static_cast<GeneralSettingsScreen*>(lv_event_get_user_data(e));
    if (!self || !self->_spinSnooze) return;
    lv_spinbox_increment(self->_spinSnooze);
    self->_resetTimer();
}

void GeneralSettingsScreen::_spinValueCb(lv_event_t* e) {
    auto* self = static_cast<GeneralSettingsScreen*>(lv_event_get_user_data(e));
    if (!self || !self->_spinSnooze) return;
    int32_t v = lv_spinbox_get_value(self->_spinSnooze);
    if (v < 1)  v = 1;
    if (v > 30) v = 30;
    g_appConfig.setSnoozeMinutes((uint16_t)v);
    self->_resetTimer();
}

void GeneralSettingsScreen::_brightnessCb(lv_event_t* e) {
    auto* self = static_cast<GeneralSettingsScreen*>(lv_event_get_user_data(e));
    if (!self || !self->_brightnessSlider) return;
    self->_resetTimer();
    if (self->_onBrightness) {
        uint8_t br = (uint8_t)lv_slider_get_value(self->_brightnessSlider);
        self->_onBrightness(br);
    }
}

void GeneralSettingsScreen::_testBtnCb(lv_event_t* e) {
    static uint32_t lastFire = 0;
    const uint32_t now = lv_tick_get();
    if (now - lastFire < 500) return;
    lastFire = now;
    auto* self = static_cast<GeneralSettingsScreen*>(lv_event_get_user_data(e));
    if (!self || !self->_alarmDropdown || !self->_onTestAlarm) return;
    self->_resetTimer();
    uint16_t idx = lv_dropdown_get_selected(self->_alarmDropdown);
    self->_onTestAlarm((size_t)idx);
}

void GeneralSettingsScreen::_timeoutCb(lv_timer_t* t) {
    auto* self = static_cast<GeneralSettingsScreen*>(lv_timer_get_user_data(t));
    if (!self) return;
    // If another screen is currently overlaying us (e.g. AlarmScreen during
    // a test alarm), don't dismiss our underlying screen — reschedule and
    // re-check after another TIMEOUT_MS so we eventually return to MainScreen
    // once the alarm is dismissed.
    if (self->_scr && lv_screen_active() != self->_scr) {
        lv_timer_delete(t);
        self->_timer = lv_timer_create(_timeoutCb, TIMEOUT_MS, self);
        lv_timer_set_repeat_count(self->_timer, 1);
        lv_timer_set_auto_delete(self->_timer, true);
        return;
    }
    self->_timer = nullptr;
    lv_timer_delete(t);
    self->_goBack();
}
