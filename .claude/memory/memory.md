# Memory Index — Radiowecker-AI-2.0

## Session Start
Read this file first. Load specific topic files only when relevant.

## Project Files

| File | Description | Last updated |
|------|-------------|--------------|
| `general.md` | Project structure, code style, architecture; class roles (MainScreen/SettingsScreen/AlarmManager/AudioPlayer/DisplayManager); main screen layout summary; OTA/NET_HOSTNAME `radiowecker2`/mDNS | 2026-05-24 |
| `domain/esp32-s3-lvgl.md` | Working toolchain (espressif32 5.4.0 + arduino-esp32 2.0.17 + LovyanGFX 1.2.7 + lvgl 9.2.2); PARTIAL flush; GT911 over Wire1; USB-CDC TX fix; OTA tearing accepted | 2026-05-21 |
| `domain/lvgl-ui.md` | LVGL 9 widget patterns; screen transitions (`lv_screen_load_anim`); **lv_lock deadlock rule**; timer API; **SettingsScreen layout + callbacks**; **monospace clock (8 fixed-width cells)**; touch dedup logging; font gen pattern; hex-escape pitfall; **font fallback must be on RAM copy (not flash const_cast)**; **German QWERTZ keyboard layout + ctrl_map rules** | 2026-05-25 |
| `domain/arduino-esp32-compat.md` | arduino-esp32 2.0.17 notes; LittleFS partition label bug; SHT31/SGP30 quirks; ESP8266Audio compat; NTP via `configTime` | 2026-05-22 |
| `domain/audio.md` | ESP8266Audio on 2.0.17; I2S pins BCLK=20/LRCLK=2/DOUT=19; audio task Core 0; SD + ICY streaming verified; URLs: SD=`/Chef316.mp3`, SRF3=`http://stream.srg-ssr.ch/m/drs3/mp3_128` | 2026-05-24 |
| `domain/wifi-captive-portal.md` | WiFiConnector, SD-based credentials, portal flow | 2026-05-21 |
| `domain/weather.md` | WeatherManager (OpenWeatherMap One Call 3.0); SD-cached PNG icons in PSRAM | 2026-05-21 |
| `tools/build.ps1` | **Use this for all PIO ops** — wraps pio.exe, handles COM kill, SUCCESS/FAILED output | 2026-05-24 |
| `tools/platformio.md` | Pre-build scripts (patch_lvgl only), manual reset, platformio.ini pointer | 2026-05-24 |
| `tools/claude-code-hooks.md` | PreToolUse memory-injection hook details | 2026-05-21 |

## Global Memory

Read claude.md for memory rules and topic files.
