# Memory Index — Radiowecker-AI-2.0

## Global Memory

Read `~/.claude/CLAUDE.md` for memory rules and topic files.

## Project Files

Read this file at session start. Load specific topic files only when relevant.

| File | Description | Last updated |
|------|-------------|--------------|
| `general.md` | Project structure, module layout, code style | 2026-05-15 |
| `domain/esp32-s3-lvgl.md` | Hardware pins, LovyanGFX 1.2.21 correct config pattern, LVGL 9 integration, library gotchas | 2026-05-15 |
| `tools/platformio.md` | PlatformIO CLI commands, build flags, LVGL ARM assembly patch | 2026-05-15 |

## Domain Knowledge Lifecycle

1. Staging — knowledge accumulates in `domain/{name}/`
2. Promotion — enough knowledge exists to package as a plugin/skill
3. Pointer — after promotion, the memory file becomes a pointer to the plugin
