// =====================================================================
//  GeneralSettingsScreen.cpp  —  "System" tab of SettingsScreen
// =====================================================================
#include "GeneralSettingsScreen.h"
#include "../AppConfig.h"
#include <stdio.h>

// External fonts (extern "C")
extern "C" const lv_font_t ui_font_ms14m;
extern "C" const lv_font_t ui_font_ms24m;

GeneralSettingsScreen generalSettingsScreen;

namespace {
constexpr uint32_t BG          = 0xF1F5F9;
constexpr uint32_t PANEL_BG    = 0xFFFFFF;
constexpr uint32_t TITLE       = 0x1E293B;
constexpr uint32_t SUB         = 0x475569;
constexpr uint32_t BORD        = 0xCBD5E1;
constexpr uint32_t INPUT_BG    = 0xF8FAFC;
constexpr uint32_t ACCENT_FILL = 0x2563EB;
constexpr uint32_t ACCENT_TXT  = 0x2563EB;
constexpr uint32_t BTN_TXT     = 0x1E293B;

void noChrome(lv_obj_t* o) {
    lv_obj_set_style_radius(o, 0, 0);
    lv_obj_set_style_border_width(o, 0, 0);
    lv_obj_set_style_pad_all(o, 0, 0);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
}

lv_obj_t* makeLabel(lv_obj_t* parent, const char* text, int x, int y,
                    const lv_font_t* font, uint32_t color) {
    lv_obj_t* l = lv_label_create(parent);
    lv_label_set_text(l, text);
    lv_obj_set_pos(l, x, y);
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, lv_color_hex(color), 0);
    return l;
}

lv_obj_t* makeBtn(lv_obj_t* parent, const char* text, int x, int y, int w, int h,
                  uint32_t fill, uint32_t border, uint32_t txt,
                  const lv_font_t* font) {
    lv_obj_t* b = lv_btn_create(parent);
    lv_obj_set_pos(b, x, y);
    lv_obj_set_size(b, w, h);
    lv_obj_set_style_radius(b, 6, 0);
    if (fill) {
        lv_obj_set_style_bg_color(b, lv_color_hex(fill), 0);
        lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(b, 0, 0);
    } else {
        lv_obj_set_style_bg_opa(b, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_color(b, lv_color_hex(border), 0);
        lv_obj_set_style_border_width(b, 1, 0);
    }
    lv_obj_t* lbl = lv_label_create(b);
    lv_label_set_text(lbl, text);
    lv_obj_center(lbl);
    lv_obj_set_style_text_color(lbl, lv_color_hex(txt), 0);
    lv_obj_set_style_text_font(lbl, font, 0);
    return b;
}

// Build a [-] [value] [+] row at row y. Label sits at (20,y).
// Spinbox (display only) is at x=380. Returns the spinbox.
lv_obj_t* buildSpinRow(lv_obj_t* parent, const char* labelText, int y,
                       int minV, int maxV, int initV,
                       lv_event_cb_t valueCb, void* user) {
    makeLabel(parent, labelText, 20, y + 8, &ui_font_ms14m, TITLE);

    lv_obj_t* dec = makeBtn(parent, "-", 360, y, 44, 40,
                            0, BORD, BTN_TXT, &ui_font_ms24m);
    lv_obj_t* spin = lv_spinbox_create(parent);
    lv_obj_set_pos(spin, 410, y);
    lv_obj_set_size(spin, 110, 40);
    lv_spinbox_set_range(spin, minV, maxV);
    lv_spinbox_set_digit_format(spin, 3, 0);
    lv_spinbox_set_step(spin, 1);
    lv_spinbox_set_value(spin, initV);
    lv_obj_set_style_bg_color(spin, lv_color_hex(INPUT_BG), 0);
    lv_obj_set_style_text_color(spin, lv_color_hex(TITLE), 0);
    lv_obj_set_style_border_color(spin, lv_color_hex(BORD), 0);
    lv_obj_set_style_border_width(spin, 1, 0);
    lv_obj_set_style_radius(spin, 6, 0);
    lv_obj_set_style_text_font(spin, &ui_font_ms24m, 0);
    lv_obj_set_style_text_align(spin, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_add_event_cb(spin, valueCb, LV_EVENT_VALUE_CHANGED, user);

    lv_obj_t* inc = makeBtn(parent, "+", 526, y, 44, 40,
                            0, BORD, BTN_TXT, &ui_font_ms24m);

    // Attach dec/inc handlers with spin pointer as user data so they can
    // step the spinbox; we use a tiny lambda-style static cb that
    // adjusts the linked spinbox.
    lv_obj_add_event_cb(dec, GeneralSettingsScreen::_spinDecCb,
                        LV_EVENT_CLICKED, spin);
    lv_obj_add_event_cb(inc, GeneralSettingsScreen::_spinIncCb,
                        LV_EVENT_CLICKED, spin);
    return spin;
}

// Build "label  [slider]  value" row. Label at (20, y), slider at (220, y+8) w=460.
lv_obj_t* buildSliderRow(lv_obj_t* parent, const char* labelText, int y,
                         uint8_t initV, lv_event_cb_t cb, void* user) {
    makeLabel(parent, labelText, 20, y, &ui_font_ms14m, TITLE);
    lv_obj_t* sl = lv_slider_create(parent);
    lv_obj_set_pos(sl, 220, y + 4);
    lv_obj_set_size(sl, 460, 16);
    lv_slider_set_range(sl, 10, 255);
    lv_slider_set_value(sl, initV, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(sl, lv_color_hex(BORD), LV_PART_MAIN);
    lv_obj_set_style_bg_color(sl, lv_color_hex(ACCENT_FILL), LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(sl, lv_color_hex(ACCENT_FILL), LV_PART_KNOB);
    lv_obj_add_event_cb(sl, cb, LV_EVENT_VALUE_CHANGED, user);
    return sl;
}
} // namespace

// ---------------------------------------------------------------------------
// create
// ---------------------------------------------------------------------------
void GeneralSettingsScreen::create(lv_obj_t* parent) {
    _root = parent;
    lv_obj_clean(parent);
    lv_obj_set_style_bg_color(parent, lv_color_hex(BG), 0);
    lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(parent, 0, 0);
    lv_obj_set_style_border_width(parent, 0, 0);
    lv_obj_clear_flag(parent, LV_OBJ_FLAG_SCROLLABLE);

    // Row 1: snooze
    _spinSnooze = buildSpinRow(parent, "Schlummerdauer (Min.)", 10,
                               1, 30, g_appConfig.snoozeMinutes(),
                               _snoozeValueCb, this);
    // Row 2: max alarm duration
    _spinMaxAlarm = buildSpinRow(parent, "Max. Alarmdauer (Min., 0=aus)", 60,
                                 0, 60, g_appConfig.maxAlarmDurationMinutes(),
                                 _maxAlarmValueCb, this);
    // Row 3: inactivity timeout
    _spinInactivity = buildSpinRow(parent, "Inaktivit\xc3\xa4t (Sek., 0=aus)", 110,
                                   0, 300, g_appConfig.inactivityTimeoutSeconds(),
                                   _inactivityValueCb, this);

    // Brightness sliders
    _sliderMain = buildSliderRow(parent, "Helligkeit Hauptbildschirm", 175,
                                 g_appConfig.mainBrightness(), _mainBriCb, this);
    _sliderAlarm = buildSliderRow(parent, "Helligkeit Weckbildschirm", 220,
                                  g_appConfig.alarmBrightness(), _alarmBriCb, this);
    _sliderSettings = buildSliderRow(parent, "Helligkeit Einstellungen", 265,
                                     g_appConfig.settingsBrightness(),
                                     _settingsBriCb, this);

    // Debug toggle
    _chkDebug = lv_checkbox_create(parent);
    lv_checkbox_set_text(_chkDebug, "Debug-Tab aktivieren (wirkt beim n\xc3\xa4""chsten \xc3\x96""ffnen)");
    lv_obj_set_pos(_chkDebug, 20, 330);
    lv_obj_set_style_text_font(_chkDebug, &ui_font_ms14m, 0);
    lv_obj_set_style_text_color(_chkDebug, lv_color_hex(TITLE), 0);
    // Indicator (the tick box itself): make it big enough to see and give it
    // a filled accent color when checked. Custom ui_font_ms14m has no ✓
    // glyph, so we rely on background fill + border instead of text.
    lv_obj_set_style_bg_color(_chkDebug, lv_color_hex(INPUT_BG), LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(_chkDebug, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_border_color(_chkDebug, lv_color_hex(BORD), LV_PART_INDICATOR);
    lv_obj_set_style_border_width(_chkDebug, 2, LV_PART_INDICATOR);
    lv_obj_set_style_radius(_chkDebug, 4, LV_PART_INDICATOR);
    lv_obj_set_style_pad_all(_chkDebug, 8, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(_chkDebug, lv_color_hex(ACCENT_FILL),
                              LV_PART_INDICATOR | LV_STATE_CHECKED);
    lv_obj_set_style_border_color(_chkDebug, lv_color_hex(ACCENT_FILL),
                                  LV_PART_INDICATOR | LV_STATE_CHECKED);
    if (g_appConfig.debugEnabled()) lv_obj_add_state(_chkDebug, LV_STATE_CHECKED);
    lv_obj_add_event_cb(_chkDebug, _debugChkCb, LV_EVENT_VALUE_CHANGED, this);
}

// ---------------------------------------------------------------------------
// Static callbacks
// ---------------------------------------------------------------------------
void GeneralSettingsScreen::_spinDecCb(lv_event_t* e) {
    auto* spin = static_cast<lv_obj_t*>(lv_event_get_user_data(e));
    if (!spin) return;
    lv_spinbox_decrement(spin);
    lv_obj_send_event(spin, LV_EVENT_VALUE_CHANGED, nullptr);
}
void GeneralSettingsScreen::_spinIncCb(lv_event_t* e) {
    auto* spin = static_cast<lv_obj_t*>(lv_event_get_user_data(e));
    if (!spin) return;
    lv_spinbox_increment(spin);
    lv_obj_send_event(spin, LV_EVENT_VALUE_CHANGED, nullptr);
}

void GeneralSettingsScreen::_snoozeValueCb(lv_event_t* e) {
    auto* self = static_cast<GeneralSettingsScreen*>(lv_event_get_user_data(e));
    if (!self || !self->_spinSnooze) return;
    g_appConfig.setSnoozeMinutes(lv_spinbox_get_value(self->_spinSnooze));
}
void GeneralSettingsScreen::_maxAlarmValueCb(lv_event_t* e) {
    auto* self = static_cast<GeneralSettingsScreen*>(lv_event_get_user_data(e));
    if (!self || !self->_spinMaxAlarm) return;
    g_appConfig.setMaxAlarmDurationMinutes(lv_spinbox_get_value(self->_spinMaxAlarm));
}
void GeneralSettingsScreen::_inactivityValueCb(lv_event_t* e) {
    auto* self = static_cast<GeneralSettingsScreen*>(lv_event_get_user_data(e));
    if (!self || !self->_spinInactivity) return;
    g_appConfig.setInactivityTimeoutSeconds(lv_spinbox_get_value(self->_spinInactivity));
}

void GeneralSettingsScreen::_mainBriCb(lv_event_t* e) {
    auto* self = static_cast<GeneralSettingsScreen*>(lv_event_get_user_data(e));
    if (!self || !self->_sliderMain) return;
    uint8_t v = (uint8_t)lv_slider_get_value(self->_sliderMain);
    g_appConfig.setMainBrightness(v);
    if (self->_onMainBri) self->_onMainBri(v);
}
void GeneralSettingsScreen::_alarmBriCb(lv_event_t* e) {
    auto* self = static_cast<GeneralSettingsScreen*>(lv_event_get_user_data(e));
    if (!self || !self->_sliderAlarm) return;
    uint8_t v = (uint8_t)lv_slider_get_value(self->_sliderAlarm);
    g_appConfig.setAlarmBrightness(v);
    if (self->_onAlarmBri) self->_onAlarmBri(v);
}
void GeneralSettingsScreen::_settingsBriCb(lv_event_t* e) {
    auto* self = static_cast<GeneralSettingsScreen*>(lv_event_get_user_data(e));
    if (!self || !self->_sliderSettings) return;
    uint8_t v = (uint8_t)lv_slider_get_value(self->_sliderSettings);
    g_appConfig.setSettingsBrightness(v);
    if (self->_onSettingsBri) self->_onSettingsBri(v);
}

void GeneralSettingsScreen::_debugChkCb(lv_event_t* e) {
    auto* self = static_cast<GeneralSettingsScreen*>(lv_event_get_user_data(e));
    if (!self || !self->_chkDebug) return;
    // Debounce: GT911 contact bounce can produce multiple VALUE_CHANGED
    // events from a single tap, leaving the state flipping unpredictably.
    static uint32_t lastFire = 0;
    const uint32_t now = lv_tick_get();
    if (now - lastFire < 350) return;
    lastFire = now;
    bool checked = lv_obj_has_state(self->_chkDebug, LV_STATE_CHECKED);
    g_appConfig.setDebugEnabled(checked);
}
