#include "AlarmSetupScreen.h"
#include <SD.h>
#include "../serial_safe.h"
#include "../audio/StationsList.h"

extern AlarmManager  alarms;
extern StationsList  g_stations;

LV_FONT_DECLARE(ui_font_ms14m)
LV_FONT_DECLARE(ui_font_ms24m)

// Global instance (declared extern in header).
AlarmSetupScreen alarmSetupScreen;

// ---------------------------------------------------------------------------
// Palette (light blue/white — mirrors examples/mockups/alarm_setup_1.png)
// ---------------------------------------------------------------------------
namespace {
constexpr uint32_t BG          = 0xF1F5F9;  // page background (slate-100)
constexpr uint32_t BAR_BG      = 0xF1F5F9;  // title bar matches page
constexpr uint32_t PANEL_BG    = 0xFFFFFF;  // card surface
constexpr uint32_t INPUT_BG    = 0xFFFFFF;  // textareas, rollers, dropdown
constexpr uint32_t TITLE       = 0x0F172A;  // bold heading text (slate-900)
constexpr uint32_t SUB         = 0x64748B;  // section labels (slate-500)
constexpr uint32_t BORD        = 0xCBD5E1;  // card / input border
constexpr uint32_t BORD_DIM    = 0xE2E8F0;  // subtle border
constexpr uint32_t TEXT        = 0x1E293B;  // body text (slate-800)
constexpr uint32_t TEXT_DIM    = 0x64748B;  // secondary text
constexpr uint32_t SEL_BG      = 0xDBEAFE;  // selected list row (blue-100)
constexpr uint32_t SEL_BORD    = 0x93C5FD;  // selected row border (blue-300)
constexpr uint32_t ACCENT_TXT  = 0x2563EB;  // links / accent text (blue-600)
constexpr uint32_t DANGER      = 0xDC2626;  // destructive (red-600)
constexpr uint32_t SAVE_FILL   = 0x2563EB;  // primary button fill (blue-600)
constexpr uint32_t SAVE_TXT    = 0xFFFFFF;
constexpr uint32_t CHIP_ON_BG  = 0x3B82F6;  // weekday active (blue-500)
constexpr uint32_t CHIP_OFF_BG = 0x475569;  // weekday inactive (slate-600)

constexpr int W = 800;
constexpr int H = 480;
constexpr int BAR_H = 54;
constexpr uint32_t TIMEOUT_MS = 30000;

const char* DAY_LABELS[7]  = { "Mo", "Di", "Mi", "Do", "Fr", "Sa", "So" };
// Map chip index -> weekday bit (0=Sun..6=Sat)
const uint8_t CHIP_TO_WDAY[7] = { 1, 2, 3, 4, 5, 6, 0 };
}

// ---------------------------------------------------------------------------
// Style helpers
// ---------------------------------------------------------------------------
static void noChrome(lv_obj_t* o) {
    lv_obj_set_style_border_width(o, 0, 0);
    lv_obj_set_style_pad_all(o, 0, 0);
    lv_obj_set_style_radius(o, 0, 0);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
}

static lv_obj_t* makePanel(lv_obj_t* p, int x, int y, int w, int h, uint32_t bg) {
    lv_obj_t* o = lv_obj_create(p);
    lv_obj_set_pos(o, x, y);
    lv_obj_set_size(o, w, h);
    lv_obj_set_style_bg_color(o, lv_color_hex(bg), 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(o, lv_color_hex(BORD_DIM), 0);
    lv_obj_set_style_border_width(o, 1, 0);
    lv_obj_set_style_radius(o, 8, 0);
    lv_obj_set_style_pad_all(o, 0, 0);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    return o;
}

static lv_obj_t* makeLabel(lv_obj_t* p, const char* text, int x, int y,
                            const lv_font_t* f, uint32_t color) {
    lv_obj_t* l = lv_label_create(p);
    lv_label_set_text(l, text);
    lv_obj_set_style_text_font(l, f, 0);
    lv_obj_set_style_text_color(l, lv_color_hex(color), 0);
    lv_obj_set_pos(l, x, y);
    return l;
}

static lv_obj_t* makeBtn(lv_obj_t* p, const char* label,
                          int x, int y, int w, int h,
                          uint32_t bg, uint32_t border, uint32_t textColor,
                          const lv_font_t* f) {
    lv_obj_t* b = lv_button_create(p);
    lv_obj_set_pos(b, x, y);
    lv_obj_set_size(b, w, h);
    lv_obj_set_style_radius(b, 6, 0);
    if (bg == 0) {
        lv_obj_set_style_bg_opa(b, LV_OPA_TRANSP, 0);
    } else {
        lv_obj_set_style_bg_color(b, lv_color_hex(bg), 0);
        lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
    }
    lv_obj_set_style_border_color(b, lv_color_hex(border), 0);
    lv_obj_set_style_border_width(b, 1, 0);
    lv_obj_set_style_pad_all(b, 0, 0);

    lv_obj_t* lbl = lv_label_create(b);
    lv_label_set_text(lbl, label);
    lv_obj_set_style_text_font(lbl, f, 0);
    lv_obj_set_style_text_color(lbl, lv_color_hex(textColor), 0);
    lv_obj_center(lbl);
    return b;
}

// ---------------------------------------------------------------------------
// SD MP3 enumeration  (recursive, depth-capped)
// ---------------------------------------------------------------------------
static bool endsWithMp3(const String& name) {
    if (name.length() < 5) return false;
    String tail = name.substring(name.length() - 4);
    tail.toLowerCase();
    return tail == ".mp3";
}

static void scanMp3(const char* dir, std::vector<String>& out,
                    int depth, size_t maxFiles) {
    if (depth < 0 || out.size() >= maxFiles) return;
    File root = SD.open(dir);
    if (!root || !root.isDirectory()) { if (root) root.close(); return; }

    File f = root.openNextFile();
    while (f) {
        // f.name() returns only the basename in current ESP32 SD lib —
        // build the absolute path ourselves so SD.open() never sees a
        // path that doesn't start with '/'.
        const char* base = f.name();
        if (base[0] == '.') { f.close(); f = root.openNextFile(); continue; }
        String fullPath = (strcmp(dir, "/") == 0)
            ? String("/") + base
            : String(dir) + "/" + base;
        if (f.isDirectory()) {
            if (depth > 0 && out.size() < maxFiles) {
                scanMp3(fullPath.c_str(), out, depth - 1, maxFiles);
            }
        } else if (endsWithMp3(fullPath)) {
            out.push_back(fullPath);
            if (out.size() >= maxFiles) { f.close(); break; }
        }
        f.close();
        f = root.openNextFile();
    }
    root.close();
}

// ---------------------------------------------------------------------------
// _rebuildDropdown
// ---------------------------------------------------------------------------
void AlarmSetupScreen::_rebuildDropdown() {
    _dropdownEntries.clear();

    // Streams from stations.json
    for (const auto& s : g_stations.stations()) {
        DropdownEntry e;
        e.kind  = DropdownKind::Stream;
        e.value = s.url;
        e.label = String("[Stream] ") + s.name;
        _dropdownEntries.push_back(e);
    }

    // SD MP3 files (cap 64)
    std::vector<String> files;
    scanMp3("/", files, /*depth=*/2, /*maxFiles=*/64);
    for (const auto& path : files) {
        DropdownEntry e;
        e.kind  = DropdownKind::SD;
        e.value = path;
        e.label = String("[SD] ") + path;
        _dropdownEntries.push_back(e);
    }

    // Build the newline-joined options string
    String opts;
    for (size_t i = 0; i < _dropdownEntries.size(); ++i) {
        if (i > 0) opts += '\n';
        opts += _dropdownEntries[i].label;
    }
    if (_dropdownEntries.empty()) {
        opts = "(keine Quelle)";
    }
    lv_dropdown_set_options(_sndDropdown, opts.c_str());
}

// ---------------------------------------------------------------------------
// _rebuildList
// ---------------------------------------------------------------------------
void AlarmSetupScreen::_rebuildList() {
    if (!_listInner) return;
    lv_obj_clean(_listInner);

    const auto& list = alarms.alarms();
    int rowY = 6;
    for (size_t i = 0; i < list.size(); ++i) {
        const Alarm& a = list[i];
        const bool selected = ((int)i == _selIndex);

        lv_obj_t* row = lv_obj_create(_listInner);
        lv_obj_set_pos(row, 6, rowY);
        lv_obj_set_size(row, 340 - 12 - 12, 64);
        lv_obj_set_style_bg_color(row, lv_color_hex(selected ? SEL_BG : PANEL_BG), 0);
        lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(row, lv_color_hex(selected ? SEL_BORD : BORD_DIM), 0);
        lv_obj_set_style_border_width(row, 1, 0);
        lv_obj_set_style_radius(row, 6, 0);
        lv_obj_set_style_pad_all(row, 0, 0);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
        // Stash row index in user data; tap = select.
        lv_obj_set_user_data(row, (void*)(intptr_t)i);
        lv_obj_add_event_cb(row, _listRowCb, LV_EVENT_CLICKED, this);

        // Title
        lv_obj_t* lblTitle = lv_label_create(row);
        const String tShow = a.title.length() ? a.title : String("(Ohne Titel)");
        lv_label_set_text(lblTitle, tShow.c_str());
        lv_obj_set_style_text_font(lblTitle, &ui_font_ms14m, 0);
        lv_obj_set_style_text_color(lblTitle, lv_color_hex(TITLE), 0);
        lv_obj_set_pos(lblTitle, 10, 8);

        // Sub: weekdays + time
        char sub[64];
        if (a.isOneShot()) {
            snprintf(sub, sizeof(sub), "Einmalig   %02d:%02d", a.hour, a.minute);
        } else {
            char wd[32] = {0};
            for (int c = 0; c < 7; ++c) {
                if (a.weekdays & (1 << CHIP_TO_WDAY[c])) {
                    if (wd[0]) strncat(wd, " ", sizeof(wd) - strlen(wd) - 1);
                    strncat(wd, DAY_LABELS[c], sizeof(wd) - strlen(wd) - 1);
                }
            }
            snprintf(sub, sizeof(sub), "%s   %02d:%02d", wd, a.hour, a.minute);
        }
        lv_obj_t* lblSub = lv_label_create(row);
        lv_label_set_text(lblSub, sub);
        lv_obj_set_style_text_font(lblSub, &ui_font_ms14m, 0);
        lv_obj_set_style_text_color(lblSub, lv_color_hex(SUB), 0);
        lv_obj_set_pos(lblSub, 10, 36);

        // Enable toggle (right side)
        lv_obj_t* tog = lv_obj_create(row);
        lv_obj_set_size(tog, 44, 22);
        lv_obj_set_pos(tog, 340 - 12 - 12 - 56, 21);
        lv_obj_set_style_radius(tog, 11, 0);
        lv_obj_set_style_bg_color(tog,
            lv_color_hex(a.enabled ? SAVE_FILL : 0x404040), 0);
        lv_obj_set_style_bg_opa(tog, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(tog, 0, 0);
        lv_obj_set_style_pad_all(tog, 0, 0);
        lv_obj_clear_flag(tog, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(tog, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_user_data(tog, (void*)(intptr_t)i);
        lv_obj_add_event_cb(tog, _toggleRowCb, LV_EVENT_CLICKED, this);

        lv_obj_t* knob = lv_obj_create(tog);
        lv_obj_set_size(knob, 18, 18);
        lv_obj_set_pos(knob, a.enabled ? 24 : 2, 2);
        lv_obj_set_style_radius(knob, 9, 0);
        lv_obj_set_style_bg_color(knob, lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_bg_opa(knob, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(knob, 0, 0);
        lv_obj_set_style_pad_all(knob, 0, 0);
        lv_obj_clear_flag(knob, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_clear_flag(knob, LV_OBJ_FLAG_CLICKABLE);  // forward taps to parent

        rowY += 72;
    }

    // When creating a new alarm (_selIndex == -1) show a placeholder so the
    // user can see that the right-panel editor is for a brand-new entry.
    if (_selIndex < 0) {
        lv_obj_t* row = lv_obj_create(_listInner);
        lv_obj_set_pos(row, 6, rowY);
        lv_obj_set_size(row, 340 - 12 - 12, 64);
        lv_obj_set_style_bg_color(row, lv_color_hex(SEL_BG), 0);
        lv_obj_set_style_bg_opa(row, LV_OPA_COVER, 0);
        lv_obj_set_style_border_color(row, lv_color_hex(SAVE_FILL), 0);
        lv_obj_set_style_border_width(row, 2, 0);
        lv_obj_set_style_radius(row, 6, 0);
        lv_obj_set_style_pad_all(row, 0, 0);
        lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_t* lbl = lv_label_create(row);
        lv_label_set_text(lbl, "+ Neuer Alarm");
        lv_obj_set_style_text_font(lbl, &ui_font_ms14m, 0);
        lv_obj_set_style_text_color(lbl, lv_color_hex(SAVE_FILL), 0);
        lv_obj_set_pos(lbl, 10, 24);
    }
}

// ---------------------------------------------------------------------------
// _setChipActive
// ---------------------------------------------------------------------------
void AlarmSetupScreen::_setChipActive(int i, bool active) {
    if (i < 0 || i >= 7 || !_chips[i]) return;
    lv_obj_set_style_bg_color(_chips[i],
        lv_color_hex(active ? CHIP_ON_BG : CHIP_OFF_BG), 0);
    lv_obj_set_style_border_color(_chips[i],
        lv_color_hex(active ? SEL_BORD : BORD_DIM), 0);
    lv_obj_t* lbl = lv_obj_get_child(_chips[i], 0);
    if (lbl) {
        lv_obj_set_style_text_color(lbl,
            lv_color_hex(active ? 0xFFFFFF : TEXT_DIM), 0);
    }
}

// ---------------------------------------------------------------------------
// _applyDraftToUI
// ---------------------------------------------------------------------------
void AlarmSetupScreen::_applyDraftToUI() {
    if (_titleBtnLabel) {
        const String t = _draft.title.length() ? _draft.title : String("(tippen zum bearbeiten)");
        lv_label_set_text(_titleBtnLabel, t.c_str());
    }
    if (_rollerHour)   lv_roller_set_selected(_rollerHour,   _draft.hour,   LV_ANIM_OFF);
    if (_rollerMinute) lv_roller_set_selected(_rollerMinute, _draft.minute, LV_ANIM_OFF);
    for (int i = 0; i < 7; ++i) {
        const bool on = (_draft.weekdays >> CHIP_TO_WDAY[i]) & 1;
        _setChipActive(i, on);
    }
    if (_volSlider) lv_slider_set_value(_volSlider, _draft.volume, LV_ANIM_OFF);
    // Sync dropdown selection
    if (_sndDropdown) {
        uint16_t sel = 0;
        for (size_t i = 0; i < _dropdownEntries.size(); ++i) {
            const auto& e = _dropdownEntries[i];
            if (e.kind == DropdownKind::Stream &&
                _draft.soundType == SoundType::Stream &&
                e.value == _draft.streamUrl) { sel = (uint16_t)i; break; }
            if (e.kind == DropdownKind::SD &&
                _draft.soundType == SoundType::SD &&
                e.value == _draft.soundPath) { sel = (uint16_t)i; break; }
        }
        lv_dropdown_set_selected(_sndDropdown, sel);
    }
}

// ---------------------------------------------------------------------------
// _writeDraftFromUI
// ---------------------------------------------------------------------------
void AlarmSetupScreen::_writeDraftFromUI() {
    if (_rollerHour)   _draft.hour   = (uint8_t)lv_roller_get_selected(_rollerHour);
    if (_rollerMinute) _draft.minute = (uint8_t)lv_roller_get_selected(_rollerMinute);
    if (_volSlider) _draft.volume = (uint8_t)lv_slider_get_value(_volSlider);

    // Weekdays from chip background color check would be fragile — instead
    // we maintain a parallel mask in _draft.weekdays at chip-click time.
    // (See _chipCb.)

    // Sound from dropdown
    if (_sndDropdown && !_dropdownEntries.empty()) {
        uint16_t idx = lv_dropdown_get_selected(_sndDropdown);
        if (idx >= _dropdownEntries.size()) idx = 0;
        const DropdownEntry& e = _dropdownEntries[idx];
        if (e.kind == DropdownKind::Stream) {
            _draft.soundType = SoundType::Stream;
            _draft.streamUrl = e.value;
            _draft.soundPath = "";
        } else {
            _draft.soundType = SoundType::SD;
            _draft.soundPath = e.value;
            _draft.streamUrl = "";
        }
    }
}

// ---------------------------------------------------------------------------
// _loadDraftFromSelection
// ---------------------------------------------------------------------------
void AlarmSetupScreen::_loadDraftFromSelection() {
    const auto& list = alarms.alarms();
    if (_selIndex >= 0 && _selIndex < (int)list.size()) {
        _draft = list[_selIndex];
        _draftIsNew = false;
    } else {
        // Empty draft for new alarm
        _draft = Alarm{};
        _draft.title    = "Neuer Alarm";
        _draft.hour     = 7;
        _draft.minute   = 0;
        _draft.weekdays = (1<<1)|(1<<2)|(1<<3)|(1<<4)|(1<<5);
        _draft.enabled  = true;
        _draft.volume   = 12;
        // Pick the first dropdown entry as default sound
        if (!_dropdownEntries.empty()) {
            const DropdownEntry& e = _dropdownEntries[0];
            if (e.kind == DropdownKind::Stream) {
                _draft.soundType = SoundType::Stream;
                _draft.streamUrl = e.value;
            } else {
                _draft.soundType = SoundType::SD;
                _draft.soundPath = e.value;
            }
        }
        _draftIsNew = true;
    }
    _applyDraftToUI();
}

// ---------------------------------------------------------------------------
// _resetTimer — mark user activity so the inactivity poller stays alive.
// LVGL automatically triggers activity on every touch through the indev,
// so this is largely redundant; we keep it for programmatic events
// (e.g. selection changes triggered without a touch).
// ---------------------------------------------------------------------------
void AlarmSetupScreen::_resetTimer() {
    lv_display_trigger_activity(NULL);
}

// ---------------------------------------------------------------------------
// _goBack
// ---------------------------------------------------------------------------
void AlarmSetupScreen::_goBack() {
    if (!_scr) return;
    if (_timer) { lv_timer_delete(_timer); _timer = nullptr; }
    lv_screen_load_anim(_mainScr, LV_SCR_LOAD_ANIM_MOVE_LEFT, 300, 0, true);
    _scr = nullptr;
    _sndDropdown = nullptr;
    _sndDropdownMask = nullptr;
    // The keyboard overlay (if any) was a child of _scr and is auto-deleted
    // along with it. Clear the dangling pointers so the next visit can open
    // the title editor again.
    _kbOverlay  = nullptr;
    _kbTextarea = nullptr;
    _keyboard   = nullptr;
}

// ---------------------------------------------------------------------------
// create()
// ---------------------------------------------------------------------------
void AlarmSetupScreen::create(lv_obj_t* mainScr) {
    // Defensive: clean up an orphaned previous screen (see GeneralSettings
    // create() for the full rationale).
    if (_scr) {
        if (_timer) { lv_timer_delete(_timer); _timer = nullptr; }
        lv_obj_delete(_scr);
        _scr = nullptr;
    }
    _mainScr = mainScr;

    _scr = lv_obj_create(NULL);
    lv_obj_set_size(_scr, W, H);
    lv_obj_set_style_bg_color(_scr, lv_color_hex(BG), 0);
    lv_obj_set_style_bg_opa(_scr, LV_OPA_COVER, 0);
    noChrome(_scr);

    // ---- Title bar ----
    lv_obj_t* bar = lv_obj_create(_scr);
    lv_obj_set_pos(bar, 0, 0);
    lv_obj_set_size(bar, W, BAR_H);
    lv_obj_set_style_bg_color(bar, lv_color_hex(BAR_BG), 0);
    lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
    noChrome(bar);

    lv_obj_t* backBtn = makeBtn(bar, "< Back", 12, 10, 90, 34,
                                 0, BORD, TEXT_DIM, &lv_font_montserrat_14);
    lv_obj_add_event_cb(backBtn, _backCb, LV_EVENT_CLICKED, this);

    makeLabel(bar, "Alarme verwalten", W/2 - 110, 13, &ui_font_ms24m, TITLE);

    lv_obj_t* newBtn = makeBtn(bar, "+ Neu", W - 112, 10, 100, 34,
                                SAVE_FILL, SAVE_FILL, SAVE_TXT,
                                &lv_font_montserrat_14);
    lv_obj_add_event_cb(newBtn, _newCb, LV_EVENT_CLICKED, this);

    // ---- Left list panel ----
    _listPanel = makePanel(_scr, 12, 66, 340, H - 78, PANEL_BG);
    _listInner = lv_obj_create(_listPanel);
    lv_obj_set_pos(_listInner, 0, 0);
    lv_obj_set_size(_listInner, 340, H - 78);
    lv_obj_set_style_bg_opa(_listInner, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(_listInner, 0, 0);
    lv_obj_set_style_pad_all(_listInner, 0, 0);
    lv_obj_set_scroll_dir(_listInner, LV_DIR_VER);

    // ---- Right editor panel ----
    constexpr int EX = 364, EY = 66, EW = W - 364 - 12, EH = H - 78;
    _editPanel = makePanel(_scr, EX, EY, EW, EH, PANEL_BG);

    // Title button (full width, top). No "Titel" label — the button text
    // shows the current title (or placeholder).
    _titleBtn = lv_button_create(_editPanel);
    lv_obj_set_pos(_titleBtn, 14, 12);
    lv_obj_set_size(_titleBtn, EW - 28, 40);
    lv_obj_set_style_radius(_titleBtn, 6, 0);
    lv_obj_set_style_bg_color(_titleBtn, lv_color_hex(INPUT_BG), 0);
    lv_obj_set_style_bg_opa(_titleBtn, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(_titleBtn, lv_color_hex(BORD_DIM), 0);
    lv_obj_set_style_border_width(_titleBtn, 1, 0);
    lv_obj_set_style_pad_all(_titleBtn, 0, 0);
    lv_obj_add_event_cb(_titleBtn, _titleBtnCb, LV_EVENT_CLICKED, this);
    _titleBtnLabel = lv_label_create(_titleBtn);
    lv_label_set_text(_titleBtnLabel, "(tippen zum bearbeiten)");
    lv_obj_set_style_text_font(_titleBtnLabel, &ui_font_ms14m, 0);
    lv_obj_set_style_text_color(_titleBtnLabel, lv_color_hex(TEXT), 0);
    lv_obj_align(_titleBtnLabel, LV_ALIGN_LEFT_MID, 10, 0);

    // Hour / minute rollers (left half). Rollers ~110px tall.
    makeLabel(_editPanel, "Uhrzeit", 14, 64, &lv_font_montserrat_14, SUB);
    _rollerHour = lv_roller_create(_editPanel);
    {
        String opts;
        for (int i = 0; i < 24; ++i) {
            if (i) opts += '\n';
            char b[4]; snprintf(b, sizeof(b), "%02d", i); opts += b;
        }
        lv_roller_set_options(_rollerHour, opts.c_str(), LV_ROLLER_MODE_NORMAL);
    }
    lv_roller_set_visible_row_count(_rollerHour, 3);
    lv_obj_set_pos(_rollerHour, 14, 84);
    lv_obj_set_width(_rollerHour, 80);
    lv_obj_set_style_bg_color(_rollerHour, lv_color_hex(INPUT_BG), 0);
    lv_obj_set_style_bg_opa(_rollerHour, LV_OPA_COVER, 0);
    lv_obj_set_style_text_font(_rollerHour, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(_rollerHour, lv_color_hex(TITLE), 0);
    lv_obj_set_style_bg_color(_rollerHour, lv_color_hex(SAVE_FILL), LV_PART_SELECTED);
    lv_obj_set_style_text_color(_rollerHour, lv_color_hex(0xFFFFFF), LV_PART_SELECTED);

    _rollerMinute = lv_roller_create(_editPanel);
    {
        String opts;
        for (int i = 0; i < 60; ++i) {
            if (i) opts += '\n';
            char b[4]; snprintf(b, sizeof(b), "%02d", i); opts += b;
        }
        lv_roller_set_options(_rollerMinute, opts.c_str(), LV_ROLLER_MODE_NORMAL);
    }
    lv_roller_set_visible_row_count(_rollerMinute, 3);
    lv_obj_set_pos(_rollerMinute, 104, 84);
    lv_obj_set_width(_rollerMinute, 80);
    lv_obj_set_style_bg_color(_rollerMinute, lv_color_hex(INPUT_BG), 0);
    lv_obj_set_style_bg_opa(_rollerMinute, LV_OPA_COVER, 0);
    lv_obj_set_style_text_font(_rollerMinute, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(_rollerMinute, lv_color_hex(TITLE), 0);
    lv_obj_set_style_bg_color(_rollerMinute, lv_color_hex(SAVE_FILL), LV_PART_SELECTED);
    lv_obj_set_style_text_color(_rollerMinute, lv_color_hex(0xFFFFFF), LV_PART_SELECTED);

    // Volume section (right half, beside rollers). Label + Test/Stop + slider.
    makeLabel(_editPanel, "Lautst\xc3\xa4" "rke", 210, 64, &ui_font_ms14m, SUB);
    {
        lv_obj_t* bTest = makeBtn(_editPanel, LV_SYMBOL_PLAY " Test",
                                   210, 84, 96, 34,
                                   0, BORD, ACCENT_TXT, &lv_font_montserrat_14);
        lv_obj_add_event_cb(bTest, _previewCb, LV_EVENT_CLICKED, this);
        lv_obj_t* bStop = makeBtn(_editPanel, LV_SYMBOL_STOP " Stop",
                                   314, 84, 96, 34,
                                   0, BORD, TEXT, &lv_font_montserrat_14);
        lv_obj_add_event_cb(bStop, _stopCb, LV_EVENT_CLICKED, this);
    }
    _volSlider = lv_slider_create(_editPanel);
    lv_obj_set_pos(_volSlider, 210, 132);
    lv_obj_set_size(_volSlider, 200, 34);
    lv_slider_set_range(_volSlider, 0, 21);
    lv_obj_set_style_bg_color(_volSlider, lv_color_hex(BORD_DIM), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(_volSlider, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_color(_volSlider, lv_color_hex(BORD), LV_PART_MAIN);
    lv_obj_set_style_border_width(_volSlider, 1, LV_PART_MAIN);
    lv_obj_set_style_radius(_volSlider, 4, LV_PART_MAIN);
    lv_obj_set_style_pad_ver(_volSlider, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_color(_volSlider, lv_color_hex(SAVE_FILL), LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(_volSlider, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_radius(_volSlider, 4, LV_PART_INDICATOR);
    lv_obj_set_style_bg_color(_volSlider, lv_color_hex(SAVE_FILL), LV_PART_KNOB);
    lv_obj_set_style_bg_opa(_volSlider, LV_OPA_COVER, LV_PART_KNOB);
    lv_obj_set_style_radius(_volSlider, 5, LV_PART_KNOB);
    lv_obj_set_style_pad_all(_volSlider, 6, LV_PART_KNOB);
    lv_obj_add_event_cb(_volSlider, _volCb, LV_EVENT_VALUE_CHANGED, this);

    // Weekday chips
    makeLabel(_editPanel, "Wochentage", 14, 206, &lv_font_montserrat_14, SUB);
    {
        const int CW = 50, CH = 36, GAP = 8;
        const int totalW = 7 * CW + 6 * GAP;
        const int chipsX = (EW - totalW) / 2;
        for (int i = 0; i < 7; ++i) {
            lv_obj_t* c = lv_button_create(_editPanel);
            lv_obj_set_pos(c, chipsX + i * (CW + GAP), 226);
            lv_obj_set_size(c, CW, CH);
            lv_obj_set_style_radius(c, 6, 0);
            lv_obj_set_style_bg_color(c, lv_color_hex(CHIP_OFF_BG), 0);
            lv_obj_set_style_bg_opa(c, LV_OPA_COVER, 0);
            lv_obj_set_style_border_color(c, lv_color_hex(BORD_DIM), 0);
            lv_obj_set_style_border_width(c, 1, 0);
            lv_obj_set_style_pad_all(c, 0, 0);
            lv_obj_set_user_data(c, (void*)(intptr_t)i);
            lv_obj_add_event_cb(c, _chipCb, LV_EVENT_CLICKED, this);
            lv_obj_t* lbl = lv_label_create(c);
            lv_label_set_text(lbl, DAY_LABELS[i]);
            lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
            lv_obj_set_style_text_color(lbl, lv_color_hex(0xFFFFFF), 0);
            lv_obj_center(lbl);
            _chips[i] = c;
        }
    }
    // Hint: no weekday selected → one-shot alarm ("Einmalig")
    makeLabel(_editPanel, "(Kein Tag ausgew\xc3\xa4" "hlt = Einmalig)",
               14, 268, &ui_font_ms14m, TEXT_DIM);

    // Sound dropdown (no "Klang" label — the dropdown is self-explanatory)
    _sndDropdown = lv_dropdown_create(_editPanel);
    lv_obj_set_pos(_sndDropdown, 14, 290);
    lv_obj_set_size(_sndDropdown, EW - 28, 40);
    lv_obj_set_style_bg_color(_sndDropdown, lv_color_hex(INPUT_BG), 0);
    lv_obj_set_style_bg_opa(_sndDropdown, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(_sndDropdown, lv_color_hex(BORD), 0);
    lv_obj_set_style_border_width(_sndDropdown, 1, 0);
    lv_obj_set_style_text_color(_sndDropdown, lv_color_hex(TEXT), 0);
    lv_obj_set_style_text_font(_sndDropdown, &ui_font_ms14m, 0);
    // Open upward so the list never escapes the screen bottom
    lv_dropdown_set_dir(_sndDropdown, LV_DIR_TOP);
    lv_dropdown_set_symbol(_sndDropdown, NULL);
    // Drop-down list styling + scroll
    lv_obj_t* dlist = lv_dropdown_get_list(_sndDropdown);
    lv_obj_set_style_bg_color(dlist, lv_color_hex(INPUT_BG), 0);
    lv_obj_set_style_text_color(dlist, lv_color_hex(TEXT), 0);
    lv_obj_set_style_text_font(dlist, &ui_font_ms14m, 0);
    lv_obj_set_style_max_height(dlist, 260, 0);
    // Ensure list is touch-scrollable
    lv_obj_add_flag(dlist, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(dlist, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(dlist, LV_SCROLLBAR_MODE_AUTO);
    // Highlight the currently-selected item even while scrolling
    lv_obj_set_style_bg_color(dlist, lv_color_hex(SEL_BG),
                              LV_PART_SELECTED | LV_STATE_CHECKED);
    lv_obj_set_style_bg_opa(dlist, LV_OPA_COVER,
                            LV_PART_SELECTED | LV_STATE_CHECKED);
    lv_obj_set_style_text_color(dlist, lv_color_hex(TITLE),
                                LV_PART_SELECTED | LV_STATE_CHECKED);

    // Modal click-blocker.
    //
    // LVGL 9's dropdown re-parents its list to the ACTIVE SCREEN when opened
    // (lv_dropdown.c line 522: lv_obj_set_parent(list, lv_obj_get_screen(...))).
    // The list ends up as the last child of _scr — z-above siblings, but only
    // covering its own bounds. Touches outside the list bounds still fall
    // through to the rollers / chips / sliders sitting behind it.
    //
    // Fix: a full-screen transparent click-trap, ALSO a child of _scr. While
    // the dropdown is open we order siblings as: [editor panel] [mask] [list].
    // The list stays on top so it scrolls; the mask absorbs every touch
    // anywhere else. Tapping the mask closes the dropdown.
    _sndDropdownMask = lv_obj_create(_scr);
    lv_obj_set_pos(_sndDropdownMask, 0, 0);
    lv_obj_set_size(_sndDropdownMask, W, H);
    lv_obj_set_style_bg_opa(_sndDropdownMask, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(_sndDropdownMask, 0, 0);
    lv_obj_set_style_pad_all(_sndDropdownMask, 0, 0);
    lv_obj_clear_flag(_sndDropdownMask, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(_sndDropdownMask, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(_sndDropdownMask, LV_OBJ_FLAG_HIDDEN);

    lv_obj_add_event_cb(_sndDropdown, _dropdownEventCb,
                        LV_EVENT_READY, this);
    lv_obj_add_event_cb(_sndDropdown, _dropdownEventCb,
                        LV_EVENT_CANCEL, this);
    lv_obj_add_event_cb(_sndDropdown, _dropdownEventCb,
                        LV_EVENT_VALUE_CHANGED, this);
    lv_obj_add_event_cb(_sndDropdownMask,
        [](lv_event_t* e) {
            auto* s = static_cast<AlarmSetupScreen*>(lv_event_get_user_data(e));
            if (s && s->_sndDropdown) lv_dropdown_close(s->_sndDropdown);
        },
        LV_EVENT_CLICKED, this);

    // Action button row at bottom — 3 buttons, evenly spaced across EW=424
    {
        const int by = EH - 46;
        // Löschen x=40 w=80 | Abbrechen x=160 w=96 | Speichern x=296 w=90
        lv_obj_t* bDel = makeBtn(_editPanel, "L\xc3\xb6schen",
                                  40, by, 80, 36,
                                  0, DANGER, DANGER, &ui_font_ms14m);
        lv_obj_add_event_cb(bDel, _deleteCb, LV_EVENT_CLICKED, this);
        lv_obj_t* bCancel = makeBtn(_editPanel, "Abbrechen",
                                     160, by, 96, 36,
                                     0, BORD, TEXT, &lv_font_montserrat_14);
        lv_obj_add_event_cb(bCancel, _cancelCb, LV_EVENT_CLICKED, this);
        lv_obj_t* bSave = makeBtn(_editPanel, "Speichern",
                                   296, by, 90, 36,
                                   SAVE_FILL, SAVE_FILL, SAVE_TXT,
                                   &lv_font_montserrat_14);
        lv_obj_add_event_cb(bSave, _saveCb, LV_EVENT_CLICKED, this);
    }

    // Populate dynamic content
    _rebuildDropdown();
    if (!alarms.alarms().empty()) {
        _selIndex = 0;
    }
    _rebuildList();
    _loadDraftFromSelection();

    // True inactivity timer: poll LVGL's input-activity counter every second.
    // Any touch anywhere (rollers, keyboard keys, dropdown items, slider drag,
    // etc.) resets the global activity timestamp automatically, so the screen
    // only returns to the main view after TIMEOUT_MS of zero interaction.
    lv_display_trigger_activity(NULL);  // start counting from "now"
    _timer = lv_timer_create(_timeoutCb, 1000, this);

    lv_screen_load_anim(_scr, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 300, 0, true);
}

// ---------------------------------------------------------------------------
// Title-edit overlay (textarea + keyboard)
// ---------------------------------------------------------------------------
void AlarmSetupScreen::_showKeyboard() {
    if (_kbOverlay) return;
    _kbOverlay = lv_obj_create(_scr);
    lv_obj_set_pos(_kbOverlay, 0, 0);
    lv_obj_set_size(_kbOverlay, W, H);
    lv_obj_set_style_bg_color(_kbOverlay, lv_color_hex(BG), 0);
    lv_obj_set_style_bg_opa(_kbOverlay, LV_OPA_COVER, 0);
    noChrome(_kbOverlay);

    // Top row: title (left) + OK/Cancel (right) so the keyboard gets the
    // entire lower portion of the screen without bottom-row clipping.
    makeLabel(_kbOverlay, "Titel bearbeiten", 12, 8, &ui_font_ms24m, TITLE);

    lv_obj_t* okBtn = makeBtn(_kbOverlay, "OK", W - 230, 6, 100, 34,
                               SAVE_FILL, SAVE_FILL, SAVE_TXT,
                               &lv_font_montserrat_14);
    lv_obj_add_event_cb(okBtn, _kbOkCb, LV_EVENT_CLICKED, this);
    lv_obj_t* cancelBtn = makeBtn(_kbOverlay, "Abbrechen", W - 120, 6, 104, 34,
                                   0, BORD, TEXT, &lv_font_montserrat_14);
    lv_obj_add_event_cb(cancelBtn, _kbCancelCb, LV_EVENT_CLICKED, this);

    // Textarea below the title row.
    _kbTextarea = lv_textarea_create(_kbOverlay);
    lv_obj_set_pos(_kbTextarea, 12, 46);
    lv_obj_set_size(_kbTextarea, W - 24, 42);
    lv_textarea_set_one_line(_kbTextarea, true);
    lv_textarea_set_max_length(_kbTextarea, 48);
    lv_textarea_set_text(_kbTextarea, _draft.title.c_str());
    lv_obj_set_style_bg_color(_kbTextarea, lv_color_hex(INPUT_BG), 0);
    lv_obj_set_style_text_color(_kbTextarea, lv_color_hex(TITLE), 0);
    lv_obj_set_style_text_font(_kbTextarea, &ui_font_ms24m, 0);
    lv_obj_set_style_border_color(_kbTextarea, lv_color_hex(BORD), 0);
    lv_obj_set_style_pad_all(_kbTextarea, 6, 0);

    // Keyboard fills the rest. y=96 → 384px tall, comfortably fits 4 rows
    // including the bottom space bar. Zero outer padding so internal rows can
    // use the full height.
    //
    // IMPORTANT: lv_keyboard_create auto-applies LV_ALIGN_BOTTOM_MID; if we
    // leave that alignment in place, set_pos is treated as an offset from the
    // bottom and the space-bar row falls off the screen. Force TOP_LEFT.
    _keyboard = lv_keyboard_create(_kbOverlay);
    lv_obj_set_align(_keyboard, LV_ALIGN_TOP_LEFT);
    lv_obj_set_pos(_keyboard, 0, 96);
    lv_obj_set_size(_keyboard, W, H - 96);
    lv_obj_set_style_pad_all(_keyboard, 2, 0);
    lv_obj_set_style_pad_row(_keyboard, 2, 0);
    lv_obj_set_style_pad_column(_keyboard, 2, 0);
    lv_keyboard_set_textarea(_keyboard, _kbTextarea);
    lv_keyboard_set_mode(_keyboard, LV_KEYBOARD_MODE_TEXT_LOWER);
}

void AlarmSetupScreen::_hideKeyboard(bool commit) {
    if (!_kbOverlay) return;
    if (commit && _kbTextarea) {
        _draft.title = lv_textarea_get_text(_kbTextarea);
        if (_titleBtnLabel) {
            const String t = _draft.title.length() ? _draft.title : String("(tippen zum bearbeiten)");
            lv_label_set_text(_titleBtnLabel, t.c_str());
        }
    }
    lv_obj_delete(_kbOverlay);
    _kbOverlay  = nullptr;
    _kbTextarea = nullptr;
    _keyboard   = nullptr;
}

// ---------------------------------------------------------------------------
// Event callbacks
// ---------------------------------------------------------------------------
void AlarmSetupScreen::_backCb(lv_event_t* e) {
    static uint32_t lastFire = 0;
    const uint32_t now = lv_tick_get();
    if (now - lastFire < 350) return;
    lastFire = now;
    auto* self = static_cast<AlarmSetupScreen*>(lv_event_get_user_data(e));
    if (self) self->_goBack();
}

void AlarmSetupScreen::_newCb(lv_event_t* e) {
    auto* self = static_cast<AlarmSetupScreen*>(lv_event_get_user_data(e));
    if (!self) return;
    static uint32_t lastFire = 0;
    const uint32_t now = lv_tick_get();
    if (now - lastFire < 350) return;
    lastFire = now;
    self->_resetTimer();
    self->_selIndex = -1;
    self->_loadDraftFromSelection();
    self->_rebuildList();
}

void AlarmSetupScreen::_listRowCb(lv_event_t* e) {
    auto* self = static_cast<AlarmSetupScreen*>(lv_event_get_user_data(e));
    if (!self) return;
    static uint32_t lastFire = 0;
    const uint32_t now = lv_tick_get();
    if (now - lastFire < 250) return;
    lastFire = now;
    self->_resetTimer();
    lv_obj_t* row = (lv_obj_t*)lv_event_get_target(e);
    int idx = (int)(intptr_t)lv_obj_get_user_data(row);
    self->_selIndex = idx;
    self->_loadDraftFromSelection();
    self->_rebuildList();
}

void AlarmSetupScreen::_toggleRowCb(lv_event_t* e) {
    auto* self = static_cast<AlarmSetupScreen*>(lv_event_get_user_data(e));
    if (!self) return;
    static uint32_t lastFire = 0;
    const uint32_t now = lv_tick_get();
    if (now - lastFire < 350) return;
    lastFire = now;
    self->_resetTimer();
    lv_obj_t* tog = (lv_obj_t*)lv_event_get_target(e);
    int idx = (int)(intptr_t)lv_obj_get_user_data(tog);
    Alarm* a = alarms.at((size_t)idx);
    if (!a) return;
    Alarm copy = *a;
    copy.enabled = !copy.enabled;
    alarms.updateAlarm((size_t)idx, copy);
    if (self->_selIndex == idx) {
        self->_draft = copy;
        self->_applyDraftToUI();
    }
    self->_rebuildList();
    if (self->_onChanged) self->_onChanged();
    // Stop event so the row click doesn't fire too.
    lv_event_stop_bubbling(e);
}

void AlarmSetupScreen::_titleBtnCb(lv_event_t* e) {
    auto* self = static_cast<AlarmSetupScreen*>(lv_event_get_user_data(e));
    if (!self) return;
    static uint32_t lastFire = 0;
    const uint32_t now = lv_tick_get();
    if (now - lastFire < 350) return;
    lastFire = now;
    self->_resetTimer();
    self->_showKeyboard();
}

void AlarmSetupScreen::_chipCb(lv_event_t* e) {
    auto* self = static_cast<AlarmSetupScreen*>(lv_event_get_user_data(e));
    if (!self) return;
    static uint32_t lastFire = 0;
    const uint32_t now = lv_tick_get();
    if (now - lastFire < 200) return;
    lastFire = now;
    self->_resetTimer();
    lv_obj_t* chip = (lv_obj_t*)lv_event_get_target(e);
    int i = (int)(intptr_t)lv_obj_get_user_data(chip);
    if (i < 0 || i >= 7) return;
    const uint8_t bit = (1u << CHIP_TO_WDAY[i]);
    const bool nowOn = !((self->_draft.weekdays >> CHIP_TO_WDAY[i]) & 1);
    if (nowOn) self->_draft.weekdays |= bit;
    else       self->_draft.weekdays &= ~bit;
    self->_setChipActive(i, nowOn);
}

void AlarmSetupScreen::_previewCb(lv_event_t* e) {
    auto* self = static_cast<AlarmSetupScreen*>(lv_event_get_user_data(e));
    if (!self) return;
    static uint32_t lastFire = 0;
    const uint32_t now = lv_tick_get();
    if (now - lastFire < 350) return;
    lastFire = now;
    self->_resetTimer();
    self->_writeDraftFromUI();
    if (self->_onVolumeChange) self->_onVolumeChange(self->_draft.volume);
    if (self->_draft.soundType == SoundType::Stream && self->_draft.streamUrl.length()) {
        if (self->_onPreviewStream) self->_onPreviewStream(self->_draft.streamUrl.c_str());
    } else if (self->_draft.soundType == SoundType::SD && self->_draft.soundPath.length()) {
        if (self->_onPreviewFile) self->_onPreviewFile(self->_draft.soundPath.c_str());
    }
}

void AlarmSetupScreen::_stopCb(lv_event_t* e) {
    auto* self = static_cast<AlarmSetupScreen*>(lv_event_get_user_data(e));
    if (!self) return;
    static uint32_t lastFire = 0;
    const uint32_t now = lv_tick_get();
    if (now - lastFire < 350) return;
    lastFire = now;
    self->_resetTimer();
    if (self->_onStop) self->_onStop();
}

void AlarmSetupScreen::_volCb(lv_event_t* e) {
    auto* self = static_cast<AlarmSetupScreen*>(lv_event_get_user_data(e));
    if (!self || !self->_volSlider) return;
    self->_resetTimer();
    const uint8_t v = (uint8_t)lv_slider_get_value(self->_volSlider);
    self->_draft.volume = v;
    if (self->_onVolumeChange) self->_onVolumeChange(v);
}

void AlarmSetupScreen::_deleteCb(lv_event_t* e) {
    auto* self = static_cast<AlarmSetupScreen*>(lv_event_get_user_data(e));
    if (!self) return;
    static uint32_t lastFire = 0;
    const uint32_t now = lv_tick_get();
    if (now - lastFire < 350) return;
    lastFire = now;
    self->_resetTimer();
    if (self->_draftIsNew) {
        // Just clear the form: select something else (first in list) or empty draft.
        self->_selIndex = alarms.alarms().empty() ? -1 : 0;
        self->_loadDraftFromSelection();
        self->_rebuildList();
        return;
    }
    if (self->_selIndex < 0) return;
    alarms.removeAlarm((size_t)self->_selIndex);
    if ((int)alarms.alarms().size() <= self->_selIndex) {
        self->_selIndex = (int)alarms.alarms().size() - 1;
    }
    self->_loadDraftFromSelection();
    self->_rebuildList();
    if (self->_onChanged) self->_onChanged();
}

void AlarmSetupScreen::_cancelCb(lv_event_t* e) {
    auto* self = static_cast<AlarmSetupScreen*>(lv_event_get_user_data(e));
    if (!self) return;
    static uint32_t lastFire = 0;
    const uint32_t now = lv_tick_get();
    if (now - lastFire < 350) return;
    lastFire = now;
    self->_resetTimer();
    // Reload draft from underlying alarm (or empty if new)
    if (self->_draftIsNew) {
        self->_selIndex = alarms.alarms().empty() ? -1 : 0;
    }
    self->_loadDraftFromSelection();
    self->_rebuildList();
}

void AlarmSetupScreen::_saveCb(lv_event_t* e) {
    auto* self = static_cast<AlarmSetupScreen*>(lv_event_get_user_data(e));
    if (!self) return;
    static uint32_t lastFire = 0;
    const uint32_t now = lv_tick_get();
    if (now - lastFire < 350) return;
    lastFire = now;
    self->_resetTimer();
    self->_writeDraftFromUI();
    if (self->_draftIsNew) {
        alarms.addAlarm(self->_draft);
        self->_selIndex = (int)alarms.alarms().size() - 1;
        self->_draftIsNew = false;
    } else if (self->_selIndex >= 0) {
        alarms.updateAlarm((size_t)self->_selIndex, self->_draft);
    }
    self->_loadDraftFromSelection();
    self->_rebuildList();
    if (self->_onChanged) self->_onChanged();
}

void AlarmSetupScreen::_kbOkCb(lv_event_t* e) {
    auto* self = static_cast<AlarmSetupScreen*>(lv_event_get_user_data(e));
    if (!self) return;
    static uint32_t lastFire = 0;
    const uint32_t now = lv_tick_get();
    if (now - lastFire < 350) return;
    lastFire = now;
    self->_resetTimer();
    self->_hideKeyboard(true);
}
void AlarmSetupScreen::_kbCancelCb(lv_event_t* e) {
    auto* self = static_cast<AlarmSetupScreen*>(lv_event_get_user_data(e));
    if (!self) return;
    static uint32_t lastFire = 0;
    const uint32_t now = lv_tick_get();
    if (now - lastFire < 350) return;
    lastFire = now;
    self->_resetTimer();
    self->_hideKeyboard(false);
}

void AlarmSetupScreen::_timeoutCb(lv_timer_t* t) {
    auto* self = static_cast<AlarmSetupScreen*>(lv_timer_get_user_data(t));
    if (!self) return;
    if (lv_display_get_inactive_time(NULL) < TIMEOUT_MS) return;
    // If another screen is currently overlaying us (e.g. AlarmScreen during a
    // ringing alarm), don't dismiss our underlying screen — just keep polling
    // until our screen is on top again.
    if (self->_scr && lv_screen_active() != self->_scr) return;
    // Fired: tear down timer and slide back.
    lv_timer_delete(self->_timer);
    self->_timer = nullptr;
    self->_goBack();
}

void AlarmSetupScreen::_dropdownEventCb(lv_event_t* e) {
    auto* self = static_cast<AlarmSetupScreen*>(lv_event_get_user_data(e));
    if (!self || !self->_sndDropdownMask) return;
    lv_event_code_t code = lv_event_get_code(e);
    // LV_EVENT_READY fires synchronously from inside lv_dropdown_open(),
    // AFTER the dropdown is marked open and the list is appended to the
    // screen. Show the mask immediately so that any phantom re-press
    // (GT911 contact bounce) lands on the mask, not on the button behind it.
    // The mask was created before the list ever joins _scr, so it is
    // naturally below the list in z-order — no move_foreground needed.
    if (code == LV_EVENT_READY) {
        lv_obj_remove_flag(self->_sndDropdownMask, LV_OBJ_FLAG_HIDDEN);
    } else if (code == LV_EVENT_CANCEL || code == LV_EVENT_VALUE_CHANGED) {
        // Closed by outside tap (CANCEL) or item selection (VALUE_CHANGED).
        lv_obj_add_flag(self->_sndDropdownMask, LV_OBJ_FLAG_HIDDEN);
    }
}
