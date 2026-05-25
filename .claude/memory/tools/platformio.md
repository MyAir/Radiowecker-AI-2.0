# Tools: PlatformIO

## 2026-05-24 — Use build.ps1 for all PIO operations

All CLI operations (build, upload, OTA, monitor, clean) are wrapped in
`.claude/tools/build.ps1`. See `claude.md` §Toolchain for usage.
Do NOT invoke `pio.exe` directly — the script handles the full exe path,
COM-port kill before USB upload, and project-root `cd`.

## 2026-05-25 — Active Pre-build Scripts

Two scripts are wired in `platformio.ini` `extra_scripts`:

```ini
extra_scripts =
    pre:scripts/patch_lvgl.py
    pre:scripts/patch_esp8266audio.py
```

- `patch_lvgl.py` — stubs LVGL 9's ARM Helium/NEON `.S` assembly files
  that fail on Xtensa.
- `patch_esp8266audio.py` — stubs `AudioFileSourceFS.cpp` and
  `AudioOutputSPIFFSWAV.cpp` (they include SPIFFS; our `include/SPIFFS.h`
  is only a shim and both files are unused). Idempotent.
- `patch_lgfx.py` is kept on disk but **not** active (its needles target
  LovyanGFX 1.2.21; we are on 1.2.7 unpatched).

## 2026-05-25 — Library patch mtime gotcha

`.pio/libdeps/{env}/` is per-env. If you ever hand-patch a library (debug
instrumentation, etc.), mirror the change to BOTH `matouch43` and
`matouch43_ota` trees and touch the file's mtime to force SCons to rebuild:

```powershell
(Get-Item .pio\libdeps\matouch43_ota\ESP8266Audio\src\AudioGeneratorMP3.cpp).LastWriteTime = Get-Date
```

Build log capture in PowerShell: `*>` writes UTF-16 — use
`2>&1 | Tee-Object -FilePath build.txt` instead. If `Get-Content` is
shadowed, use `Microsoft.PowerShell.Management\Get-Content`.

## 2026-05-15 — Manual Reset via Serial Monitor

The board resets on the **falling edge of DTR**.
In the PlatformIO monitor: press `Ctrl+T` then `Ctrl+D`.
May need to do it twice (once to set DTR active, once to release/fall).

## 2026-05-15 — Key platformio.ini Settings

Canonical build configuration (board, flags, partitions, OTA env) lives in
`domain/esp32-s3-lvgl.md` 2026-05-20 entry. Don't duplicate here.
