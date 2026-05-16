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
