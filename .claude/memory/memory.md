# Memory Index — Radiowecker-AI-2.0

## Session Start
Read this file first. Load specific topic files only when relevant.

## Project Files

| File | Description | Last updated |
|------|-------------|--------------|
| `general.md` | Project structure, code style, architecture, main screen layout, key patterns | 2026-05-18 |
| `domain/esp32-s3-lvgl.md` | Hardware pins, LovyanGFX config, LVGL 9 solution, Bus_RGB patches 5–8, 39 Hz frame rate, scramble fix, `_renderTask`, flush callback, **oscillation fix = LVGL DIRECT mode + both PSRAM framebuffers (2026-05-19)**, **I2C_NUM_1 conflict RESOLVED via manual GT911-over-Wire1 (Option 1)**, USB-CDC `setTxBufferSize(2048)` to stop first-char drops | 2026-05-19 |
| `domain/lvgl-ui.md` | LVGL 9 widget patterns: container reset, labels, divider, fonts, style tips | 2026-05-16 |
| `domain/arduino-esp32-compat.md` | Arduino-ESP32 3.x breaking changes, sensor libs, ESP8266Audio patch, SPIFFS, NTP/DNS, LittleFS | 2026-05-16 |
| `domain/wifi-captive-portal.md` | WiFiConnector class, SD-based credentials, portal flow, captive portal routes | 2026-05-16 |
| `tools/platformio.md` | PlatformIO CLI, build flags, patch scripts, PIO exe path, manual reset, **platform version (pioarduino 54.03.21), esptool/click fix** | 2026-05-18 |

## Global Memory

Read claude.md for memory rules and topic files.
