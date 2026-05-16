# Memory Index — Radiowecker-AI-2.0

## Global Memory

Read `~/.claude/CLAUDE.md` for memory rules and topic files.

## Project Files

Read this file at session start. Load specific topic files only when relevant.

| File | Description | Last updated |
|------|-------------|--------------|
| `general.md` | Project structure, module layout, code style | 2026-05-15 |
| `domain/esp32-s3-lvgl.md` | Hardware pins, LovyanGFX config, LVGL 9 integration, ADC pin limits, LVGL timer pattern, partition layout | 2026-05-16 |
| `domain/arduino-esp32-compat.md` | Library compat: Arduino-ESP32 3.x breaking changes, ESP8266Audio patch, SPIFFS removal, NTP/DNS fix, LittleFS first-boot | 2026-05-16 |
| `domain/wifi-captive-portal.md` | WiFi captive portal pattern: WiFiConnector class, SD-based credentials, portal flow, routes | 2026-05-16 |
| `tools/platformio.md` | PlatformIO CLI commands, build flags, LVGL/ESP8266Audio patch scripts, PIO exe path | 2026-05-16 |

## Domain Knowledge Lifecycle

1. Staging — knowledge accumulates in `domain/{name}/`
2. Promotion — enough knowledge exists to package as a plugin/skill
3. Pointer — after promotion, the memory file becomes a pointer to the plugin
