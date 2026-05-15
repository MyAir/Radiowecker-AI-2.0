# Memory Index — Radiowecker-AI-2.0

## Global Memory

Read `~/.claude/CLAUDE.md` for memory rules and topic files.

## Project Files

Read this file at session start. Load specific topic files only when relevant.

| File | Description | Last updated |
|------|-------------|--------------|
| `general.md` | Project conventions, naming patterns, workflow | 2026-05-15 |
| `domain/esp32-s3-lvgl.md` | Hardware pins, LVGL 9 + LovyanGFX stack, sensor wiring | 2026-05-15 |
| `tools/platformio.md` | PlatformIO CLI commands, build flags, upload patterns | 2026-05-15 |

## Domain Knowledge Lifecycle

1. Staging — knowledge accumulates in `domain/{name}/`
2. Promotion — enough knowledge exists to package as a plugin/skill
3. Pointer — after promotion, the memory file becomes a pointer to the plugin
