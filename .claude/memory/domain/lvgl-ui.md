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
