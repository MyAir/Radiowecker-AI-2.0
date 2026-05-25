#include "SettingsScreen.h"
#include "../AppConfig.h"
#include "AlarmSetupScreen.h"
#include "GeneralSettingsScreen.h"
#include "WeatherSettingsPanel.h"
#include "DebugScreen.h"

SettingsScreen settingsScreen;

extern "C" const lv_font_t ui_font_ms14m;
extern "C" const lv_font_t ui_font_ms24m;

// Palette
static constexpr uint32_t SC_BG       = 0xF1F5F9;
static constexpr uint32_t SC_TAB_BG   = 0xFFFFFF;
static constexpr uint32_t SC_TAB_INA  = 0x64748B;
static constexpr uint32_t SC_TAB_ACT  = 0x2563EB;
static constexpr uint32_t SC_BTN_BORD = 0xCBD5E1;
static constexpr uint32_t SC_BACK_TXT = 0x1E293B;

static constexpr int SCREEN_W  = 800;
static constexpr int SCREEN_H  = 480;
static constexpr int TAB_H     = 50;

// ---------------------------------------------------------------------------
// _goBack
// ---------------------------------------------------------------------------
void SettingsScreen::_goBack() {
    if (!_scr) return;
    if (_timer) { lv_timer_delete(_timer); _timer = nullptr; }
    // Child screens cached pointers into the soon-to-be-deleted tab tree.
    // Null those caches so the next open doesn't dereference dangling memory.
    debugScreen.invalidate();
    weatherSettingsPanel.invalidate();
    _tabAlarm = _tabSystem = _tabWeather = _tabDebug = nullptr;
    _tabview  = nullptr;
    _backBtn  = nullptr;
    lv_screen_load_anim(_mainScr, LV_SCR_LOAD_ANIM_MOVE_LEFT, 300, 0, true);
    _scr = nullptr;
}

// ---------------------------------------------------------------------------
// create
// ---------------------------------------------------------------------------
void SettingsScreen::create(lv_obj_t* mainScr) {
    // If a previous SettingsScreen was overlayed by AlarmScreen and the user
    // returned to MainScreen via AlarmScreen::hide(), our _scr is orphaned
    // (no longer the active LVGL screen, but still allocated). Tear it
    // down before rebuilding so the cog button works again.
    if (_scr) {
        if (lv_screen_active() == _scr) return;   // already showing → no-op
        if (_timer) { lv_timer_delete(_timer); _timer = nullptr; }
        debugScreen.invalidate();
        weatherSettingsPanel.invalidate();
        lv_obj_delete(_scr);
        _scr      = nullptr;
        _tabAlarm = _tabSystem = _tabWeather = _tabDebug = nullptr;
        _tabview  = nullptr;
        _backBtn  = nullptr;
    }
    _mainScr = mainScr;

    _scr = lv_obj_create(NULL);
    lv_obj_set_size(_scr, SCREEN_W, SCREEN_H);
    lv_obj_set_style_bg_color(_scr, lv_color_hex(SC_BG), 0);
    lv_obj_set_style_bg_opa(_scr, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(_scr, 0, 0);
    lv_obj_set_style_border_width(_scr, 0, 0);
    lv_obj_clear_flag(_scr, LV_OBJ_FLAG_SCROLLABLE);

    // ---- Tabview ----
    // NOTE: the Back button MUST NOT be a child of the tab bar — that
    //       confuses lv_tabview's internal button indexing and shifts the
    //       tab contents. We keep the tabview full-width, pad the tab bar
    //       on the left to clear space, then overlay the back button on _scr.
    static constexpr int BACK_W = 110;
    _tabview = lv_tabview_create(_scr);
    lv_tabview_set_tab_bar_position(_tabview, LV_DIR_TOP);
    lv_tabview_set_tab_bar_size(_tabview, TAB_H);
    lv_obj_set_size(_tabview, SCREEN_W, SCREEN_H);
    lv_obj_set_pos(_tabview, 0, 0);
    lv_obj_set_style_bg_color(_tabview, lv_color_hex(SC_BG), 0);
    lv_obj_set_style_bg_opa(_tabview, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(_tabview, 0, 0);
    lv_obj_set_style_pad_all(_tabview, 0, 0);

    // Style the tab buttons strip
    lv_obj_t* tabBar = lv_tabview_get_tab_bar(_tabview);
    lv_obj_set_style_bg_color(tabBar, lv_color_hex(SC_TAB_BG), 0);
    lv_obj_set_style_bg_opa(tabBar, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(tabBar, lv_color_hex(SC_BTN_BORD), 0);
    lv_obj_set_style_border_width(tabBar, 0, 0);
    lv_obj_set_style_border_side(tabBar, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_pad_all(tabBar, 0, 0);
    lv_obj_set_style_pad_left(tabBar, BACK_W, 0);   // reserve space for Back btn
    lv_obj_set_style_text_font(tabBar, &ui_font_ms24m, 0);
    lv_obj_set_style_text_color(tabBar, lv_color_hex(SC_TAB_INA), 0);
    lv_obj_set_style_text_color(tabBar, lv_color_hex(SC_TAB_ACT),
                                LV_PART_ITEMS | LV_STATE_CHECKED);

    // Back button overlay — child of _scr (NOT tabBar), drawn on top of
    // the reserved left strip of the tab bar.
    _backBtn = lv_button_create(_scr);
    lv_obj_set_size(_backBtn, BACK_W - 16, TAB_H - 12);
    lv_obj_set_pos(_backBtn, 8, 6);
    lv_obj_set_style_bg_color(_backBtn, lv_color_hex(SC_TAB_BG), 0);
    lv_obj_set_style_bg_opa(_backBtn, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(_backBtn, lv_color_hex(SC_BTN_BORD), 0);
    lv_obj_set_style_border_width(_backBtn, 1, 0);
    lv_obj_set_style_radius(_backBtn, 6, 0);
    lv_obj_set_style_pad_all(_backBtn, 0, 0);
    lv_obj_add_event_cb(_backBtn, _backBtnCb, LV_EVENT_CLICKED, this);
    lv_obj_t* lblBack = lv_label_create(_backBtn);
    lv_label_set_text(lblBack, "< Zur\xc3\xbcck");
    lv_obj_set_style_text_font(lblBack, &ui_font_ms14m, 0);
    lv_obj_set_style_text_color(lblBack, lv_color_hex(SC_BACK_TXT), 0);
    lv_obj_center(lblBack);

    // ---- Tabs ----
    _tabAlarm = lv_tabview_add_tab(_tabview, "Alarm");
    lv_obj_set_style_pad_all(_tabAlarm, 0, 0);
    lv_obj_clear_flag(_tabAlarm, LV_OBJ_FLAG_SCROLLABLE);

    _tabSystem = lv_tabview_add_tab(_tabview, "System");
    lv_obj_set_style_pad_all(_tabSystem, 0, 0);
    lv_obj_clear_flag(_tabSystem, LV_OBJ_FLAG_SCROLLABLE);

    // Populate child panels
    alarmSetupScreen.create(_tabAlarm);
    generalSettingsScreen.create(_tabSystem);

    // "Wetter" tab — always present, between System and the optional Debug tab.
    _tabWeather = lv_tabview_add_tab(_tabview, "Wetter");
    lv_obj_set_style_pad_all(_tabWeather, 0, 0);
    lv_obj_clear_flag(_tabWeather, LV_OBJ_FLAG_SCROLLABLE);
    weatherSettingsPanel.create(_tabWeather);
    weatherSettingsPanel.requestRefresh();

    if (g_appConfig.debugEnabled()) {
        _tabDebug = lv_tabview_add_tab(_tabview, "Debug");
        lv_obj_set_style_pad_all(_tabDebug, 0, 0);
        lv_obj_clear_flag(_tabDebug, LV_OBJ_FLAG_SCROLLABLE);
        debugScreen.create(_tabDebug);
    }

    // Inactivity timer (polls LVGL display activity)
    if (g_appConfig.inactivityTimeoutSeconds() > 0) {
        lv_display_trigger_activity(NULL);
        _timer = lv_timer_create(_timeoutCb, 1000, this);
    }

    lv_screen_load_anim(_scr, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 300, 0, false);
}

// ---------------------------------------------------------------------------
// Callbacks
// ---------------------------------------------------------------------------
void SettingsScreen::_backBtnCb(lv_event_t* e) {
    static uint32_t lastFire = 0;
    const uint32_t now = lv_tick_get();
    if (now - lastFire < 400) return;
    lastFire = now;
    auto* self = static_cast<SettingsScreen*>(lv_event_get_user_data(e));
    if (self) self->_goBack();
}

void SettingsScreen::_timeoutCb(lv_timer_t* t) {
    auto* self = static_cast<SettingsScreen*>(lv_timer_get_user_data(t));
    if (!self) return;
    const uint32_t timeoutMs = (uint32_t)g_appConfig.inactivityTimeoutSeconds() * 1000;
    if (timeoutMs == 0) return;
    // If another screen overlays us (e.g. AlarmScreen during a ringing
    // alarm), keep polling but don't dismiss.
    if (self->_scr && lv_screen_active() != self->_scr) return;
    if (lv_display_get_inactive_time(NULL) < timeoutMs) return;
    lv_timer_delete(self->_timer);
    self->_timer = nullptr;
    self->_goBack();
}
