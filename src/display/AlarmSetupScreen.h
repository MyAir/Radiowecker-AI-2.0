#pragma once
#include <Arduino.h>
#include <lvgl.h>
#include <vector>
#include "../alarm/AlarmManager.h"

/**
 * AlarmSetupScreen
 *
 * Two-pane editor (matches mockup alarm_setup_1):
 *   - Left list:  all alarms, tap to select. "+ Neu" header button adds one.
 *   - Right pane: editor for currently selected alarm. Title, hour/minute
 *                 rollers, weekday chips, sound dropdown, volume slider,
 *                 Probe/Stop/Löschen/Abbrechen/Speichern buttons.
 *
 * Lifetime: caller calls create(mainScr) from within an LVGL event callback.
 * The screen slides in over mainScr. Back button (or 30 s inactivity)
 * returns to mainScr; the screen is auto-deleted by LVGL.
 *
 * Reads/writes:
 *   - alarms (AlarmManager) — for CRUD
 *   - g_stations            — for the Stream half of the sound dropdown
 *   - SD card               — recursively enumerates *.mp3
 */
class AlarmSetupScreen {
public:
    using Callback        = void(*)();
    using StreamCallback  = void(*)(const char* url);
    using FileCallback    = void(*)(const char* path);
    using VolumeCallback  = void(*)(uint8_t vol);

    /** Build and slide in over mainScr. */
    void create(lv_obj_t* mainScr);

    void setOnPreviewStream(StreamCallback cb) { _onPreviewStream = cb; }
    void setOnPreviewFile  (FileCallback cb)   { _onPreviewFile   = cb; }
    void setOnStop         (Callback cb)       { _onStop          = cb; }
    void setOnVolumeChange (VolumeCallback cb) { _onVolumeChange  = cb; }
    /** Notified after Speichern, so the main screen can refresh its label. */
    void setOnChanged      (Callback cb)       { _onChanged       = cb; }

private:
    enum class DropdownKind : uint8_t { Stream, SD };
    struct DropdownEntry {
        DropdownKind kind;
        String       value;   // URL or SD path
        String       label;   // display string
    };

    lv_obj_t*   _scr      = nullptr;
    lv_obj_t*   _mainScr  = nullptr;
    lv_timer_t* _timer    = nullptr;

    // List pane
    lv_obj_t*   _listPanel  = nullptr;
    lv_obj_t*   _listInner  = nullptr;
    int         _selIndex   = -1;     // index in alarms, -1 = nothing selected
    bool        _draftIsNew = false;  // true for unsaved new alarms

    // Editor pane widgets
    lv_obj_t* _editPanel       = nullptr;
    lv_obj_t* _titleBtn        = nullptr;
    lv_obj_t* _titleBtnLabel   = nullptr;
    lv_obj_t* _rollerHour      = nullptr;
    lv_obj_t* _rollerMinute    = nullptr;
    lv_obj_t* _chips[7]        = {};
    lv_obj_t* _sndDropdown     = nullptr;
    lv_obj_t* _sndDropdownMask = nullptr;
    lv_obj_t* _volSlider       = nullptr;

    // Title-edit overlay
    lv_obj_t* _kbOverlay    = nullptr;
    lv_obj_t* _kbTextarea   = nullptr;
    lv_obj_t* _keyboard     = nullptr;

    // Working draft (mirrors editor fields).
    Alarm _draft;

    // Dropdown content (kept in sync with the lv_dropdown options string).
    std::vector<DropdownEntry> _dropdownEntries;

    StreamCallback _onPreviewStream = nullptr;
    FileCallback   _onPreviewFile   = nullptr;
    Callback       _onStop          = nullptr;
    VolumeCallback _onVolumeChange  = nullptr;
    Callback       _onChanged       = nullptr;

    void _goBack();
    void _resetTimer();
    void _rebuildDropdown();
    void _rebuildList();
    void _loadDraftFromSelection();
    void _writeDraftFromUI();
    void _applyDraftToUI();
    void _setChipActive(int i, bool active);
    void _showKeyboard();
    void _hideKeyboard(bool commit);

    static void _backCb(lv_event_t* e);
    static void _newCb(lv_event_t* e);
    static void _listRowCb(lv_event_t* e);
    static void _toggleRowCb(lv_event_t* e);
    static void _titleBtnCb(lv_event_t* e);
    static void _chipCb(lv_event_t* e);
    static void _volCb(lv_event_t* e);
    static void _previewCb(lv_event_t* e);
    static void _stopCb(lv_event_t* e);
    static void _deleteCb(lv_event_t* e);
    static void _cancelCb(lv_event_t* e);
    static void _saveCb(lv_event_t* e);
    static void _kbOkCb(lv_event_t* e);
    static void _kbCancelCb(lv_event_t* e);
    static void _dropdownEventCb(lv_event_t* e);
    static void _timeoutCb(lv_timer_t* t);
};

extern AlarmSetupScreen alarmSetupScreen;
