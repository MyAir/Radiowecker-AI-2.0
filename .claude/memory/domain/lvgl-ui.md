# Domain: LVGL 9 UI Patterns

## 2026-05-16 — Widget Patterns

### Container reset (neutralize default theme)
```cpp
lv_obj_set_style_border_width(obj, 0, 0);
lv_obj_set_style_pad_all(obj, 0, 0);
lv_obj_set_style_radius(obj, 0, 0);
lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
```
Without this, containers inherit theme padding/border — breaks pixel-precise layout.

### Transparent background
Use `LV_OPA_TRANSP` for `bg_opa` style — do NOT set bg_color to black (it will still draw).

### Label patterns
- Color: `lv_obj_set_style_text_color(lbl, lv_color_hex(0xAAAAAA), 0)`
- Align: `lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, 0)` + `lv_obj_align(lbl, LV_ALIGN_TOP_MID, 0, yOffset)`
- Update: `lv_label_set_text_fmt(lbl, "%02d:%02d:%02d", t.tm_hour, t.tm_min, t.tm_sec)`

### Divider line
Create `lv_obj_t*`, set size to (w, 1), bg_color + no border + no radius.

### Font declaration
`extern const lv_font_t lv_font_montserrat_48;` — defined by LVGL when enabled in lv_conf.h

### Misc
- Full style override needed on `lv_scr_act()` for clean dark-theme look
- Split hex-escaped strings: `"Temp: " "\xc2\xb0" "C"` not `"...\xc2\xb0C"`

---

## 2026-05-23 — MainScreen layout (confirmed working, good visual result)

Screen 800×480. Panel pos(0,28) size(800,408) — between status bar (h=28) and sensor strip (h=44).

### Vertical layout (y = offset within panel)
| Widget | y | Font | Notes |
|--------|---|------|-------|
| Weekday | 20 | ui_font_ms28m (28 px) | centered, line_h≈33 |
| Date | 63 | ui_font_ms36m (36 px) | centered, line_h≈42, supports 0x20–0xFF (März ✓) |
| Time | 158 | ui_font_ms120m (120 px) | centered; vertically centered between date-bottom (≈105) and divider (338) |
| Divider | 338 | — | x=80, w=640, h=1 |
| Alarm row | 351/358 | ui_font_ms24m (24 px) | see below |

### Alarm row (y=351 buttons, y=358 labels) — centered: x=80 to x=720 (640 px, 80 px margins)
| Element | x | w |
|---------|---|---|
| Prev button | 80 | 80 |
| "Nächster Alarm:" | 170 | 230 |
| alarm value label | 410 | 220 |
| Next button | 640 | 80 |

### Custom fonts (all Montserrat Medium, BPP4, generated with lv_font_conv 1.5.3)
- `ui_font_ms14m` — 14 px, 0x20–0xFF
- `ui_font_ms24m` — 24 px, 0x20–0xFF
- `ui_font_ms28m` — 28 px, 0x20–0xFF
- `ui_font_ms36m` — 36 px, 0x20–0xFF
- `ui_font_ms80m` — 80 px, 0x20–0x7F (legacy, kept)
- `ui_font_ms120m` — 120 px, 0x20–0x7F
Font source: `SD-Data/assets/Montserrat-Medium.ttf`
Generation pattern: `lv_font_conv --no-compress --no-prefilter --bpp 4 --size N --font Montserrat-Medium.ttf --range 0x20-0xff --lv-include lvgl.h --lv-font-name ui_font_msNNm -o src/display/ui_font_msNNm.c`

### C hex-escape pitfall
`"\xc3\xa4chster"` is WRONG — `c` extends `\xa4` to `\xa4c` (3 hex digits, out of range).
Fix: `"N\xc3\xa4" "chster Alarm:"` (adjacent string literal terminates the escape).

---

## 2026-05-24 — Screen transitions (LVGL 9)

- API: `lv_screen_load_anim(scr, anim, time_ms, delay_ms, auto_del)` — LVGL 9 name.
  (`lv_scr_load_anim` is the LVGL 8 name — do not use.)
- `auto_del=true` deletes the **previously active** screen after animation completes.
- Direction constants:
  - `LV_SCR_LOAD_ANIM_MOVE_RIGHT` — new screen enters from the left (slide right)
  - `LV_SCR_LOAD_ANIM_MOVE_LEFT`  — new screen enters from the right (slide left)
- Creating a new screen: `lv_obj_create(NULL)` in LVGL 9.

---

## 2026-05-24 — lv_lock() / FreeRTOS deadlock rule

`LV_USE_OS = LV_OS_FREERTOS` makes `lv_lock()` a real FreeRTOS mutex.
`lv_timer_handler()` acquires it at line 81 before dispatching events.
**NEVER call `lv_lock()` from within LVGL event callbacks** (or functions exclusively
called from callbacks) — this deadlocks.
Only call `lv_lock()` / `lv_unlock()` from outside LVGL context (e.g. from `loop()`
or another FreeRTOS task).

---

## 2026-05-24 — LVGL 9 timer API (confirmed)

- `lv_timer_create(cb, period_ms, user_data)` → `lv_timer_t*`
- `lv_timer_delete(timer)`, `lv_timer_reset(timer)`, `lv_timer_pause(timer)`
- `lv_timer_set_repeat_count(timer, n)` — use `1` for one-shot
- `lv_timer_set_auto_delete(timer, bool)` — auto-delete after last fire
- `lv_timer_get_user_data(timer)` → `void*`
- **Safe pattern for a one-shot timeout callback that calls `_goBack()`:**
  Set `_timer = nullptr` FIRST inside the callback (timer auto-deletes after callback
  returns), then call `_goBack()` which checks `if (_timer)` before deleting.

---

## 2026-05-24 — SettingsScreen (implemented, working)

File: `src/display/SettingsScreen.h` / `.cpp`

### Callbacks wired in main.cpp setOnSettings lambda
```cpp
settingsScreen.setOnPlaySD(cb);
settingsScreen.setOnPlaySRF3(cb);
settingsScreen.setOnStop(cb);
settingsScreen.setOnVolumeChange(cb);       // uint8_t vol
settingsScreen.setOnBrightnessChange(cb);   // uint8_t brightness
settingsScreen.create(mainScreen.screen(), audio.volume(), s_brightness);
```
- `mainScreen.screen()` returns `_scr` (lv_obj_t* of main screen, stored in `create()`).
- `s_brightness = 128` (matches DisplayManager::begin() startup value).

### Layout (800×480, y absolute on screen)
```
Title bar:     y=0..54   h=55  bg=#121212
  Back btn:    pos(12,10)  size(90,34)
  Title:       centered "Einstellungen"  montserrat_24
Divider:       y=55
KONFIGURATION: y=68
  General:     pos(60,88)   size(280,50)  mock/dim
  Alarms:      pos(370,88)  size(280,50)  mock/dim
Divider:       y=152
AUDIO:         y=160
  SD MP3:      pos(60,180)  size(205,50)  working
  SRF 3:       pos(290,180) size(205,50)  working
  Stop:        pos(520,180) size(205,50)  working
Divider:       y=244
VOLUME:        y=252
  Slider:      pos(60,278)  size(680,32)  range 0-21
Divider:       y=322
HELLIGKEIT:    y=330
  Slider:      pos(60,356)  size(680,32)  range 10-255
```
- Inactivity timeout: 30 s one-shot timer → `_goBack()`.
- Debounce 500 ms on: back btn, SD, SRF3, Stop.
- Audio URLs: SD=`/Chef316.mp3`, SRF3=`http://stream.srg-ssr.ch/m/drs3/mp3_128` (HTTP, not HTTPS).

---

## 2026-05-24 — Monospace clock digits

The time `HH:MM:SS` is split into 8 individual fixed-width labels (one per char).
Each is centered in its cell. Colons are static (created once, never updated).
Only the 6 digit labels get `lv_label_set_text()` each second.

```
Cell layout (x, w) on 800px panel — total 540px, start x=130:
  [130,78] [208,78] [286,36] [322,78] [400,78] [478,36] [514,78] [592,78]
    H         H        :       M         M        :        S         S
```
Member: `lv_obj_t* _timeDigits[8] = {}` replaces `lv_obj_t* _lblTime`.
Update loop skips indices 2 and 5 (colons).

---

## 2026-05-24 — Touch debug deduplication

`LOG_TOUCH 1` in `config.h`. Suppresses repeated lines while finger is held still.
File-scope statics `s_last_log_x / s_last_log_y` (0xFFFF initial). Only logs when
coords change. Reset to 0xFFFF on release so next press always logs.
`[Touch] release` printed once on lift.

## 2026-05-25 — lv_font_t fallback chain on ESP32 (DO NOT write to flash)

LVGL 9 supports glyph fallback via `lv_font_t::fallback`. The obvious pattern
`const_cast<lv_font_t*>(&ui_font_ms14m)->fallback = &lv_font_montserrat_14;`
**crashes silently on ESP32** because generated font structs live in `.rodata`
(flash). The store triggers LoadStoreProhibited → silent reboot (no panic
visible because watchdog reboots before UART flush at CORE_DEBUG_LEVEL=1).

Working pattern (`src/display/keyboard_de.h::fontWithFallback`):
```cpp
inline const lv_font_t* fontWithFallback() {
    static lv_font_t f;          // RAM-resident copy
    static bool inited = false;
    if (!inited) {
        f = ui_font_ms14m;                       // shallow-copy from flash
        f.fallback = &lv_font_montserrat_14;     // safe: writing to RAM
        inited = true;
    }
    return &f;
}
```
Use the returned pointer wherever you'd normally pass `&ui_font_msNNm`.

## 2026-05-25 — German QWERTZ keyboard for lv_keyboard

`src/display/keyboard_de.h` provides a custom QWERTZ layout with ä ö ü ß plus
the matching ctrl_map. **ctrl_map row counts MUST exactly equal the button
counts of the visible map** (`\n` separates rows, `""` terminates). Off-by-one
walks past the array in `lv_keyboard_update_ctrl_map` → silent reboot.
Current counts: 13 / 12 / 12 / 5 = 42 buttons.

Special button text strings (`"abc"`, `"ABC"`, `"1#"`, `LV_SYMBOL_CLOSE`,
`LV_SYMBOL_OK`, `LV_SYMBOL_LEFT`, `LV_SYMBOL_RIGHT`, `LV_SYMBOL_BACKSPACE`)
are matched by `strcmp` in LVGL's default keyboard event cb — never replace
them with plain text or the mode-switch / OK / close handling breaks.

Combine with `fontWithFallback()` so Latin-1 glyphs (from ms14m) and
LV_SYMBOL_* (from Montserrat) both render. Apply via:
```cpp
kb_de::applyGermanLayout(kb);            // font + maps
kb_de::applyVisibleCursor(textarea);     // dodger-blue blinking cursor
```
