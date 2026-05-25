// =====================================================================
//  DebugScreen.cpp  —  "Debug" tab of SettingsScreen
// =====================================================================
#include "DebugScreen.h"

extern "C" const lv_font_t ui_font_ms14m;
extern "C" const lv_font_t ui_font_ms24m;

DebugScreen debugScreen;

namespace {
constexpr uint32_t BG       = 0xF1F5F9;
constexpr uint32_t TITLE    = 0x1E293B;
constexpr uint32_t BORD     = 0xCBD5E1;
constexpr uint32_t INPUT_BG = 0xF8FAFC;
constexpr uint32_t BTN_FILL = 0x2563EB;
constexpr uint32_t BTN_TXT  = 0xFFFFFF;
}

void DebugScreen::create(lv_obj_t* parent) {
    _root = parent;
    lv_obj_clean(parent);
    lv_obj_set_style_bg_color(parent, lv_color_hex(BG), 0);
    lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(parent, 0, 0);
    lv_obj_set_style_border_width(parent, 0, 0);
    lv_obj_clear_flag(parent, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t* lbl = lv_label_create(parent);
    lv_label_set_text(lbl, "Wecker ausl\xc3\xb6sen");
    lv_obj_set_pos(lbl, 40, 24);
    lv_obj_set_style_text_font(lbl, &ui_font_ms24m, 0);
    lv_obj_set_style_text_color(lbl, lv_color_hex(TITLE), 0);

    _alarmDrop = lv_dropdown_create(parent);
    lv_obj_set_pos(_alarmDrop, 40, 70);
    lv_obj_set_size(_alarmDrop, 510, 60);
    lv_dropdown_set_options_static(_alarmDrop, "(keine)");
    // ui_font_ms24m has no LV_SYMBOL_DOWN (U+F078) glyph → drop the arrow.
    lv_dropdown_set_symbol(_alarmDrop, NULL);
    lv_obj_set_style_bg_color(_alarmDrop, lv_color_hex(INPUT_BG), 0);
    lv_obj_set_style_border_color(_alarmDrop, lv_color_hex(BORD), 0);
    lv_obj_set_style_border_width(_alarmDrop, 1, 0);
    lv_obj_set_style_radius(_alarmDrop, 6, 0);
    lv_obj_set_style_text_font(_alarmDrop, &ui_font_ms24m, 0);
    lv_obj_set_style_text_color(_alarmDrop, lv_color_hex(TITLE), 0);

    _testBtn = lv_btn_create(parent);
    lv_obj_set_pos(_testBtn, 570, 70);
    lv_obj_set_size(_testBtn, 190, 60);
    lv_obj_set_style_radius(_testBtn, 6, 0);
    lv_obj_set_style_bg_color(_testBtn, lv_color_hex(BTN_FILL), 0);
    lv_obj_set_style_bg_opa(_testBtn, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(_testBtn, 0, 0);
    lv_obj_t* btnLbl = lv_label_create(_testBtn);
    lv_label_set_text(btnLbl, "Testen");
    lv_obj_center(btnLbl);
    lv_obj_set_style_text_font(btnLbl, &ui_font_ms24m, 0);
    lv_obj_set_style_text_color(btnLbl, lv_color_hex(BTN_TXT), 0);
    lv_obj_add_event_cb(_testBtn, _testBtnCb, LV_EVENT_CLICKED, this);
}

void DebugScreen::setAlarmOptions(const char* options) {
    if (!_alarmDrop) return;
    if (!options || !*options) {
        lv_dropdown_set_options_static(_alarmDrop, "(keine)");
        lv_dropdown_set_selected(_alarmDrop, 0);
        return;
    }
    lv_dropdown_set_options(_alarmDrop, options);
    lv_dropdown_set_selected(_alarmDrop, 0);
}

void DebugScreen::_testBtnCb(lv_event_t* e) {
    auto* self = static_cast<DebugScreen*>(lv_event_get_user_data(e));
    if (!self || !self->_alarmDrop) return;
    if (!self->_onTestAlarm) return;
    size_t idx = (size_t)lv_dropdown_get_selected(self->_alarmDrop);
    self->_onTestAlarm(idx);
}
