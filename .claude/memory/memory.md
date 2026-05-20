# Memory Index — Radiowecker-AI-2.0

## Session Start
Read this file first. Load specific topic files only when relevant.

## Project Files

| File | Description | Last updated |
|------|-------------|--------------|
| `general.md` | Project structure, code style, architecture, main screen layout, key patterns | 2026-05-18 |
| `domain/esp32-s3-lvgl.md` | **CURRENT working stack (2026-05-20): espressif32 5.4.0 + arduino-esp32 2.0.17 + LovyanGFX 1.2.7 + lvgl 9.2.2, EEZ-style writePixels flush in PARTIAL mode, LV_COLOR_16_SWAP=1**; manual GT911 over Wire1; USB-CDC TX buffer fix; LVGL fallback buffer pattern; LovyanGFX RGB config pattern; LVGL 9 tick + callback signatures; board pinout (Makerfabs MaTouch 4.3"); 16 MB partition layout | 2026-05-20 |
| `domain/lvgl-ui.md` | LVGL 9 widget patterns: container reset, labels, divider, fonts, style tips | 2026-05-16 |
| `domain/arduino-esp32-compat.md` | Banner: project on arduino-esp32 2.0.17 (3.x notes are upgrade-only); LittleFS partition label bug; SHT31/SGP30 lib quirks; 3.x breaking changes; ESP8266Audio patch (audio currently stubbed); NTP via `configTime`; LittleFS first-boot [E] is expected | 2026-05-20 |
| `domain/wifi-captive-portal.md` | WiFiConnector class, SD-based credentials, portal flow, captive portal routes | 2026-05-16 |
| `tools/platformio.md` | PlatformIO CLI, build flags, patch scripts (only `patch_lvgl.py` active), PIO exe path, manual reset; pioarduino note marked superseded | 2026-05-20 |

## Global Memory

Read claude.md for memory rules and topic files.
