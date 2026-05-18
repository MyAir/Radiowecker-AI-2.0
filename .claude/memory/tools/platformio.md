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

**Correct platform**: `pioarduino 54.03.21` (Arduino ESP32 3.x / ESP-IDF 5.x).  
`espressif32 @5.4.0` (Arduino ESP32 2.0.6 / ESP-IDF 4.4.x) is **too old** — lacks `esp_cache.h`.

```ini
platform = https://github.com/pioarduino/platform-espressif32/releases/download/54.03.21/platform-espressif32.zip
```

**esptool 5.0.0 + click ≥ 8.2 crash**:  
`esptool/cli_util.py` calls `get_metavar(None)` but click 8.2+ requires `get_metavar(param, ctx)`.  
Fix via pre-build script `scripts/patch_esptool.py` (already in repo) that replaces the call with
a `try/except TypeError` block covering both old and new click. Also deletes `__pycache__/cli_util*.pyc`
so the patched source is picked up immediately.

Add to `extra_scripts` **first**, before other patch scripts:
```ini
extra_scripts =
    pre:scripts/patch_esptool.py
    pre:scripts/patch_lvgl.py
    ...
```

## 2026-05-16 — PIO Executable Path (Windows PowerShell)

`pio` is not on the system PATH in all PowerShell sessions. Use the full path:

```powershell
& "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe" run
```
