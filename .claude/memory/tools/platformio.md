# Tools: PlatformIO

## 2026-05-15 — CLI Commands (Windows PowerShell)

| Task | Command |
|------|---------|
| Build | `pio run` |
| Upload | `try { taskkill /F /IM pio.exe /T > $null 2>&1 } catch { } ; pio run -t upload` |
| Upload filesystem | `pio run -t uploadfs` |
| Serial monitor | `pio run -t monitor` |
| Stop monitor | Ctrl+C |
| Clean | `pio run -t clean` |

Kill before upload because PIO monitor locks the COM port on Windows.

## 2026-05-15 — Key platformio.ini Settings for This Project

```ini
board                           = esp32-s3-devkitc-1
board_build.flash_size          = 16MB
board_build.arduino.memory_type = qio_opi      ; OPI PSRAM
board_build.partitions          = partitions.csv
upload_speed                    = 921600
monitor_filters                 = esp32_exception_decoder, default
lib_archive                     = no           ; required for LovyanGFX

build_flags =
    -DBOARD_HAS_PSRAM
    -DCONFIG_SPIRAM_SUPPORT=1
    -DLV_CONF_INCLUDE_SIMPLE    ; lv_conf.h from include/
    -DLGFX_USE_V1
```

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

## 2026-05-16 — Multiple Pre-build Scripts

> **2026-05-20**: only `patch_lvgl.py` is currently active (project reverted
> to LovyanGFX 1.2.7 + arduino-esp32 2.0.17 + audio stubbed). Keep
> `patch_lgfx.py` and `patch_esp8266audio.py` on disk for reference but do
> NOT add them to `extra_scripts` \u2014 their needles target 1.2.21 / ESP8266Audio.

When more than one `extra_scripts` entry is needed, use multi-line format:

```ini
extra_scripts =
    pre:scripts/patch_lvgl.py
    pre:scripts/patch_esp8266audio.py
    pre:scripts/patch_lgfx.py
```

- `patch_esp8266audio.py` — fixes ESP8266Audio 1.9.9 `NetworkClient`/SPIFFS issues; see `domain/arduino-esp32-compat.md`
- `patch_lgfx.py` — patches `Bus_RGB.cpp` to call `lgfx_vsync_callback()` from the VSYNC_END ISR; enables the binary VSYNC semaphore in `DisplayManager`

## 2026-05-18 — Platform Version & esptool/click Incompatibility

> **Superseded 2026-05-20**: project reverted to `espressif32 @ 5.4.0` +
> `arduino-esp32 2.0.17` to fix the per-second horizontal-shift display glitch.
> See `domain/esp32-s3-lvgl.md` 2026-05-20 entry. The notes below are kept for
> historical reference only.

**Current platform**: `pioarduino 55.03.38-1` (Arduino ESP32 3.3.8 / ESP-IDF 5.5.4 / esptool 5.2.0).  
`espressif32 @5.4.0` (Arduino ESP32 2.0.6 / ESP-IDF 4.4.x) is **too old** — lacks `esp_cache.h`.  
Official `espressif32 @7.0.1` is also too old for Arduino (still 2.0.17/IDF 4.4.7). Use pioarduino.

```ini
platform = https://github.com/pioarduino/platform-espressif32/releases/download/55.03.38-1/platform-espressif32.zip
```

**esptool 5.0.0 + click ≥ 8.2 crash** (historical, now resolved):  
`esptool/cli_util.py` calls `get_metavar(None)` but click 8.2+ requires `get_metavar(param, ctx)`.  
pioarduino 54.03.21 shipped esptool 5.0.0 (pre-click-8.2), 55.03.38-1 ships esptool 5.2.0 (compatible).  
If ever downgrading to 54.03.21: add `scripts/patch_esptool.py` first in `extra_scripts`.

## 2026-05-16 — PIO Executable Path (Windows PowerShell)

`pio` is not on the system PATH in all PowerShell sessions. Use the full path:

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" run
```
