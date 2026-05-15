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
Add to `platformio.ini`: `extra_scripts = pre:scripts/patch_lvgl.py`
