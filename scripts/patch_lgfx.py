"""
patch_lgfx.py — PlatformIO extra_script (pre:)

LovyanGFX 1.2.x on ESP-IDF >= 5.4.4 defines LGFX_INTR_FLAGS as
ESP_INTR_FLAG_INTRDISABLED only (dropping ESP_INTR_FLAG_SHARED).
This is correct for the SPI bus drivers (which reject SHARED in IDF 5.4.4+),
but Bus_RGB.cpp uses the same flag when allocating the LCD_CAM interrupt.
Without ESP_INTR_FLAG_SHARED, LCD_CAM needs a *dedicated* interrupt slot.
On ESP32-S3 with USB CDC enabled, all suitable slots are taken and the
allocation fails, so the VSYNC ISR never fires, the GDMA doesn't restart
each frame, and the display stays black.

Fix: re-add ESP_INTR_FLAG_SHARED specifically for the isr_flags variable in
Bus_RGB.cpp so the LCD_CAM interrupt can share an already-allocated slot.
This does not affect any SPI bus initialisation.

The patch is idempotent.
"""

import os
Import("env")  # noqa: F821  (SCons magic)

bus_rgb_path = os.path.join(
    env.subst("$PROJECT_DIR"),
    ".pio", "libdeps",
    env.subst("$PIOENV"),
    "LovyanGFX", "src", "lgfx", "v1", "platforms", "esp32s3", "Bus_RGB.cpp",
)

NEEDLE      = "    int isr_flags = LGFX_INTR_FLAGS; // ESP_INTR_FLAG_INTRDISABLED | ESP_INTR_FLAG_SHARED;"
REPLACEMENT = "    int isr_flags = LGFX_INTR_FLAGS | ESP_INTR_FLAG_SHARED; // patched: re-add SHARED for LCD_CAM to avoid 'No free interrupt inputs' on ESP32-S3 with USB CDC"

if not os.path.isfile(bus_rgb_path):
    print("patch_lgfx.py: Bus_RGB.cpp not found, skipping")
else:
    with open(bus_rgb_path, "r", encoding="utf-8") as fh:
        content = fh.read()
    if NEEDLE not in content:
        pass  # already patched or pattern not found
    else:
        print("patch_lgfx.py: patching Bus_RGB.cpp — adding ESP_INTR_FLAG_SHARED to LCD_CAM isr_flags")
        with open(bus_rgb_path, "w", encoding="utf-8") as fh:
            fh.write(content.replace(NEEDLE, REPLACEMENT, 1))
