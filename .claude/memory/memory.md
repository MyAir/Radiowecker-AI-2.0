# Memory Index — Radiowecker-AI-2.0

## Session Start
Read this file first. Load specific topic files only when relevant.

## Project Files

| File | Description | Last updated |
|------|-------------|--------------|
| `general.md` | Project structure, code style, architecture, main screen layout, key patterns; **OTA / NET_HOSTNAME `radiowecker2` / mDNS** | 2026-05-21 |
| `domain/esp32-s3-lvgl.md` | **CURRENT working stack (2026-05-20): espressif32 5.4.0 + arduino-esp32 2.0.17 + LovyanGFX 1.2.7 + lvgl 9.2.2, EEZ-style writePixels flush in PARTIAL mode, LV_COLOR_16_SWAP=1**; manual GT911 over Wire1; USB-CDC TX buffer fix; LVGL fallback buffer pattern; LovyanGFX RGB config pattern; LVGL 9 tick + callback signatures; board pinout (Makerfabs MaTouch 4.3"); 16 MB partition layout; **OTA flash-write tearing is hardware-level (PSRAM FB + cache disable), accepted as-is** | 2026-05-21 |
| `domain/lvgl-ui.md` | LVGL 9 widget patterns: container reset, labels, divider, fonts, style tips | 2026-05-16 |
| `domain/arduino-esp32-compat.md` | Banner: project on arduino-esp32 2.0.17 (3.x notes are upgrade-only); LittleFS partition label bug; SHT31/SGP30 lib quirks; 3.x breaking changes; ESP8266Audio compatible on 2.0.17 (no patch needed); NTP via `configTime`; LittleFS first-boot [E] is expected | 2026-05-22 |
| `domain/audio.md` | ESP8266Audio live on 2.0.17: onboard speaker amp pins BCLK=20/LRCLK=2/DOUT=19 (LRCLK↔DOUT swap = white noise on beats); audio FreeRTOS task; ID3v2 skip in AudioPlayer for libmad BUFLEN loop; per-env libdeps + mtime gotcha | 2026-05-22 |
| `domain/wifi-captive-portal.md` | WiFiConnector class, SD-based credentials, portal flow, captive portal routes (Arduino-ESP32 2.0.17) | 2026-05-21 |
| `domain/weather.md` | WeatherManager (OpenWeatherMap One Call 3.0, 5-min poll, ArduinoJson Filter); LV_USE_LODEPNG=1; SD-cached PNG icons in PSRAM via `lv_image_dsc_t` RAW_ALPHA; tile titles current/Vormittag/Nachmittag/Morgen | 2026-05-21 |
| `tools/platformio.md` | PlatformIO CLI (incl. OTA upload), only `patch_lvgl.py` active, PIO exe path, manual reset; canonical build flags live in esp32-s3-lvgl.md | 2026-05-21 |
| `tools/claude-code-hooks.md` | PreToolUse memory-injection hook: settings.json, sh+py wrappers, PPID one-shot flag, Windows bash/python gotchas, smoke test | 2026-05-21 |

## Global Memory

Read claude.md for memory rules and topic files.
