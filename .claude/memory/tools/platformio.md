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

## 2026-10-25 � Device firmware vs repo (branch weather-panel, 33cb42b)

- Device was built with env `matouch43_ota` (libdeps paths in the image); compare against that env, not `matouch43`.
- weather-panel build == device except one extra route on the device: `/api/files/music` (not in any branch, stash or history). Device = weather-panel + uncommitted/other-folder changes (music file API). Byte-exact match not achievable (paths/layout shift).
- Compare method: dump flash, parse esp image length, diff printable strings with env names normalised.

## 2026-10-25 � Backup + /api/files/music added (follow-up to entry above)

- Device backup: `backup/device-firmware-2026-10-25/` (full 16 MB dump, per-partition bins, MANIFEST.txt with sha256 + restore commands). `backup/` is git-ignored because nvs.bin holds WiFi credentials.
- Added `GET /api/files/music` to WebUiServer (`scanMp3`: depth 2, max 64, skips dotfiles, case-insensitive .mp3 � same as on-device dropdowns). Returns `{"files":["/a.mp3",...]}`.
- After adding it, matouch43_ota build is 2300944 B vs device app1 2300576 B and the readable-string diff is empty (only binary noise left).
