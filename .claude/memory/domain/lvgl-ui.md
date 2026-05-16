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
