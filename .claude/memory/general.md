# General — Radiowecker-AI-2.0 Conventions

## 2026-05-21 — OTA / Network Identity

- Module instance: `OtaManager ota;` in `main.cpp` (alongside `display`,
  `network`, `timeManager`, `audio`, `alarms`, `sensors`).
- `NET_HOSTNAME "radiowecker2"` defined in `src/config.h`. Set on the
  WiFi interface (`WiFi.setHostname`) and used as ArduinoOTA hostname →
  `radiowecker2.local` via mDNS.
- `loop()` calls `ota.loop()` after `network.loop()`. While
  `ota.isUpdating()` is true, only `display.loop()` runs; touch / audio /
  time / alarm / sensor work is skipped to free CPU and avoid Wire1
  contention. (Hardware-level glitch during OTA is unavoidable —
  `domain/esp32-s3-lvgl.md` 2026-05-21.)
- Build envs: `[env:matouch43]` (default, USB-CDC serial @ 921600);
  `[env:matouch43_ota]` extends it with `upload_protocol = espota`,
  `upload_port = radiowecker2.local`.

## 2026-05-16 — Project Architecture

### Class Roles (updated 2026-05-24)
- `DisplayManager` — LGFX + LVGL init, `loop()` calls `lv_timer_handler()`, `pollTouch()` called from main loop, `setBrightness(uint8_t)`.
- `MainScreen` — main clock UI. `create()`, `screen()` → `lv_obj_t*`, `updateTime()`, `updateWifi()`, `updateSensors()`, `setNextAlarm()`, `setAlarmEnabled(bool)`. Callbacks: `setOnSkipAlarm`, `setOnPrevAlarm`, `setOnSettings`, `setOnAlarmToggle`.
- `SettingsScreen` — slides in over main screen. `create(mainScr, vol, brightness)`. Callbacks: `setOnPlaySD`, `setOnPlaySRF3`, `setOnStop`, `setOnVolumeChange`, `setOnBrightnessChange`. 30 s inactivity timeout.
- `AlarmManager` — load/save via SD (`/alarms.json`). `begin()`, `check(tm&)`, `setMasterEnabled(bool)`, `isMasterEnabled()`. Persists `masterEnabled` in JSON.
- `AudioPlayer` — FreeRTOS task Core 0. `playFile(path)`, `playStream(url)`, `stop()`, `setVolume(uint8_t)`, `volume() const`.
- `NetworkManager`, `OtaManager`, `TimeManager`, `SensorManager`, `WeatherManager` — as before.

### Main Screen Layout (800×480) — current (2026-05-24)
See `domain/lvgl-ui.md` for full widget layout. Summary:
- Status bar: y=0, h=28, bg=#1A1A1A
- Clock panel: pos(0,28) size(800,408)
- Sensor strip: y=436, h=44
- Corner buttons: 85×85 px; top-left = cogwheel (→ SettingsScreen), top-right = bell (alarm toggle)
- Time: 8 individual fixed-width labels for monospace rendering (see lvgl-ui.md 2026-05-24)
- Alarm toggle: `setAlarmEnabled(bool)` updates bell icon + diagonal strikethrough line.

### Key Patterns
- WiFi quality: `quality = 2 * (RSSI + 100)`, clamp 0–100
- German weekday lookup: wday 0=Sonntag … 6=Samstag
- UTF-8: degree sign = `"\xc2\xb0\x43"`, ä = `\xc3\xa4`, ü = `\xc3\xbc`
- WiFi SSID: capture `WiFi.SSID()` in `String` before `.c_str()` (avoids dangling pointer)

## 2026-05-15 — Code Style
- C++17, `#pragma once` guards, `nullptr` not `NULL`
- Prefer `static_cast<>` over C-style casts
- Module instances declared in `main.cpp`, passed by reference where needed
- Debug output via `serial_safe_printf("[Module] message\n")` — use `serial_safe.h` wrapper (thread-safe)

## 2026-05-15 — Naming & Structure
- Module headers in `include/` (hardware config, LGFX, lv_conf), source in `src/`
- One subdirectory per module: `alarm/`, `audio/`, `display/`, `network/`, `sensors/`, `time/`
- All pin/address defines live in `include/HardwareConfig.h` — never scatter magic numbers
- UI screens are built directly with LVGL v9 API inside `src/display/` — no EEZ Studio
