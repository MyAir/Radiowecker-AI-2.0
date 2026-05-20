# Tools: PlatformIO

## 2026-05-16 — PIO Executable Path (Windows PowerShell)

`pio` is not on the system PATH in all PowerShell sessions. Use the full path:

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" run
```

## 2026-05-16 — Pre-build Scripts

> **2026-05-20**: only `patch_lvgl.py` is active. `patch_lgfx.py` and
> `patch_esp8266audio.py` are kept on disk for reference but their needles
> target LovyanGFX 1.2.21 / ESP8266Audio (now dropped). Do NOT add them to
> `extra_scripts`.

```ini
extra_scripts = pre:scripts/patch_lvgl.py
```

## 2026-05-15 — CLI Commands (Windows PowerShell)

| Task | Command |
|------|---------|
| Build | `pio run` |
| Upload | `try { taskkill /F /IM pio.exe /T > $null 2>&1 } catch { } ; pio run -t upload` |
| Upload filesystem | `pio run -t uploadfs` |
| Serial monitor | `pio run -t monitor` |
| Stop monitor | Ctrl+C |
| Clean | `pio run -t clean` |
| OTA upload | `pio run -t upload -e matouch43_ota` |

Kill before upload because PIO monitor locks the COM port on Windows.

## 2026-05-15 — Key platformio.ini Settings

Canonical build configuration (board, flags, partitions, OTA env) lives in
`domain/esp32-s3-lvgl.md` 2026-05-20 entry. Don't duplicate here.

## 2026-05-15 — Manual Reset via Serial Monitor

The board resets on the **falling edge of DTR**.
In the PlatformIO monitor: press `Ctrl+T` then `Ctrl+D`.
May need to do it twice (once to set DTR active, once to release/fall).

## 2026-05-15 — LVGL ARM Assembly Patch

LVGL 9.x ships ARM Helium + NEON `.S` files that fail on Xtensa. Pre-build script:

```python
# scripts/patch_lvgl.py
import os
Import("env")
ARM_ASM_STUBS = {
    os.path.join("draw","sw","blend","helium","lv_blend_helium.S"): "/* stub */\n",
    os.path.join("draw","sw","blend","neon","lv_blend_neon.S"):     "/* stub */\n",
}
lvgl_src = os.path.join(env.subst("$PROJECT_DIR"),".pio","libdeps",env.subst("$PIOENV"),"lvgl","src")
for rel, stub in ARM_ASM_STUBS.items():
    full = os.path.join(lvgl_src, rel)
    if os.path.isfile(full) and open(full).read().strip() != stub.strip():
        open(full,"w").write(stub)
```
