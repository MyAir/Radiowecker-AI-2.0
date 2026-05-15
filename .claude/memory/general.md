# General — Radiowecker-AI-2.0 Conventions

## 2026-05-15 — Naming & Structure
- Module headers in `include/` (hardware config, LGFX, lv_conf), source in `src/`
- One subdirectory per module: `alarm/`, `audio/`, `display/`, `network/`, `sensors/`, `time/`
- All pin/address defines live in `include/HardwareConfig.h` — never scatter magic numbers
- EEZ Studio generated UI lives in `lib/ui/` — never edit manually
- User actions from EEZ Studio are prefixed `action_` in C++ (e.g. EEZ action `foo` → `action_foo()`)

## 2026-05-15 — Code Style
- C++17, `#pragma once` guards, `nullptr` not `NULL`
- Prefer `static_cast<>` over C-style casts
- Module instances declared in `main.cpp`, passed by reference where needed
- Debug output via `Serial.printf("[Module] message\n")`
