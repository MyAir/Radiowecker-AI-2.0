# Tools: PlatformIO

## 2026-05-24 — Use build.ps1 for all PIO operations

All CLI operations (build, upload, OTA, monitor, clean) are wrapped in
`.claude/tools/build.ps1`. See `claude.md` §Toolchain for usage.
Do NOT invoke `pio.exe` directly — the script handles the full exe path,
COM-port kill before USB upload, and project-root `cd`.

## 2026-05-16 — Pre-build Scripts

Only `patch_lvgl.py` is active. `patch_lgfx.py` and `patch_esp8266audio.py`
are kept on disk but target older library versions (LovyanGFX 1.2.21 /
ESP8266Audio) — do NOT add them to `extra_scripts`.

```ini
extra_scripts = pre:scripts/patch_lvgl.py
```

Full patch code lives in `scripts/patch_lvgl.py`. It stubs out LVGL 9's
ARM Helium/NEON `.S` assembly files that fail on Xtensa.

## 2026-05-15 — Manual Reset via Serial Monitor

The board resets on the **falling edge of DTR**.
In the PlatformIO monitor: press `Ctrl+T` then `Ctrl+D`.
May need to do it twice (once to set DTR active, once to release/fall).

## 2026-05-15 — Key platformio.ini Settings

Canonical build configuration (board, flags, partitions, OTA env) lives in
`domain/esp32-s3-lvgl.md` 2026-05-20 entry. Don't duplicate here.
