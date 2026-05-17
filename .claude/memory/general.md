# General — Radiowecker-AI-2.0 Conventions

## 2026-05-15 — Naming & Structure
- Module headers in `include/` (hardware config, LGFX, lv_conf), source in `src/`
- One subdirectory per module: `alarm/`, `audio/`, `display/`, `network/`, `sensors/`, `time/`
- All pin/address defines live in `include/HardwareConfig.h` — never scatter magic numbers
- UI screens are built directly with LVGL v9 API inside `src/display/` — no EEZ Studio

## 2026-05-15 — Code Style
- C++17, `#pragma once` guards, `nullptr` not `NULL`
- Prefer `static_cast<>` over C-style casts
- Module instances declared in `main.cpp`, passed by reference where needed
- Debug output via `Serial.printf("[Module] message\n")`

## 2026-05-16 — Project Architecture

### Class Roles (updated 2026-05-18)
- `DisplayManager::begin()` — inits LGFX + LVGL, `lv_tick_set_cb(millis)`, allocs 40-line PSRAM render buf, creates VSYNC binary semaphore
- `DisplayManager::loop()` — takes VSYNC semaphore (non-blocking), calls `lv_timer_handler()` once per VSYNC (~60 Hz)
- `MainScreen` — owns all LVGL widgets; `create()`, `updateTime(tm&)`, `updateWifi(ssid, ip, pct)`

### Main Screen Layout (800×480)
- Status bar: y=0, h=28, bg=#1A1A1A, text=#AAAAAA, font 14
- Clock panel: x=0, y=28, w=580, h=370 — date(24), time(48), next alarm(16), bg=#8B2020 text
- Sensor strip: x=0, y=398, w=580, h=82 — TEMP, HUM, CO2, TVOC columns
- Weather panel: x=580, y=28, w=220, h=452 — 4 stacked tiles (current h=140, 3× forecast h=100)

### Key Patterns
- WiFi quality: `quality = 2 * (RSSI + 100)`, clamp 0–100
- German weekday lookup: wday 0=Sonntag … 6=Samstag
- UTF-8: degree sign = `"\xc2\xb0\x43"`, ä = `\xc3\xa4`, ü = `\xc3\xbc`
- WiFi SSID: capture `WiFi.SSID()` in `String` before `.c_str()` (avoids dangling pointer)
