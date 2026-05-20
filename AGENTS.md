# Project Agent Instructions — Radiowecker-AI-2.0

At the start of every task, read [`.claude/memory/memory.md`](.claude/memory/memory.md)
and load the relevant topic files from `.claude/memory/{general,domain,tools}/*.md`
before making changes.

Memory rules and the GitHub Copilot override clause live in
[`claude.md`](claude.md). In short:

- Use `.claude/memory/` directly with file editing tools — do NOT use the
  internal `memory` tool (its store is non-persistent for this workspace).
- When you learn something worth remembering, write it to the right topic file
  immediately and update the index in `memory.md`.
- Before removing or modifying any existing memory entry, confirm with the user.
- "Reorganize memory" follows the 7-step protocol in `claude.md`.

## Build / Upload Cheatsheet

See [`.claude/memory/tools/platformio.md`](.claude/memory/tools/platformio.md)
for the full list. Most common:

- Build: `pio run`
- USB upload (kill monitor first): `try { taskkill /F /IM pio.exe /T > $null 2>&1 } catch { } ; pio run -t upload`
- OTA upload: `pio run -t upload -e matouch43_ota`
- Monitor: `pio run -t monitor`

## Hardware Quick Reference

Makerfabs MaTouch ESP32-S3 Parallel TFT 4.3" V3.1 — 800×480 ST7262 RGB,
GT911 touch on Wire1, 16 MB QIO flash, 8 MB OPI PSRAM. Hostname
`radiowecker2` (mDNS `radiowecker2.local`). Stack details and pinout in
[`.claude/memory/domain/esp32-s3-lvgl.md`](.claude/memory/domain/esp32-s3-lvgl.md).
