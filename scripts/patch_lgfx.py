"""
patch_lgfx.py — PlatformIO extra_script (pre:)

Patch 1 — ESP_INTR_FLAG_SHARED:
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

Patch 2 — VSYNC callback hook:
  The LCD_CAM VSYNC_END interrupt fires when GDMA restarts scanning from the
  top of the framebuffer (~every 16.7 ms at 60 Hz).  Adding a weak
  lgfx_vsync_callback() call there lets DisplayManager::loop() synchronise
  lv_refr_now() to this event: rendering starts right after DMA restarts,
  giving Cache_WriteBack_Addr a full frame budget before GDMA reaches any
  dirty UI region — eliminating the CPU-cache / DMA race that produces
  garbled text and horizontal jitter.

Both patches are idempotent.
"""

import os
Import("env")  # noqa: F821  (SCons magic)

bus_rgb_path = os.path.join(
    env.subst("$PROJECT_DIR"),
    ".pio", "libdeps",
    env.subst("$PIOENV"),
    "LovyanGFX", "src", "lgfx", "v1", "platforms", "esp32s3", "Bus_RGB.cpp",
)

# ---------------------------------------------------------------------------
# Patch 1 — ESP_INTR_FLAG_SHARED
# ---------------------------------------------------------------------------
NEEDLE1      = "    int isr_flags = LGFX_INTR_FLAGS; // ESP_INTR_FLAG_INTRDISABLED | ESP_INTR_FLAG_SHARED;"
REPLACEMENT1 = "    int isr_flags = LGFX_INTR_FLAGS | ESP_INTR_FLAG_SHARED; // patched: re-add SHARED for LCD_CAM to avoid 'No free interrupt inputs' on ESP32-S3 with USB CDC"

# ---------------------------------------------------------------------------
# Patch 2a — weak lgfx_vsync_callback() declaration (before namespace lgfx)
# ---------------------------------------------------------------------------
NEEDLE2      = "namespace lgfx\n{"
REPLACEMENT2 = (
    "// patched: weak VSYNC hook — called from lcd_default_isr_handler at VSYNC_END\n"
    "extern \"C\" __attribute__((weak)) void lgfx_vsync_callback() {}\n"
    "\n"
    "namespace lgfx\n{"
)

# ---------------------------------------------------------------------------
# Patch 2b — call lgfx_vsync_callback() in the VSYNC_END ISR block
# ---------------------------------------------------------------------------
NEEDLE3      = (
    "      GDMA.channel[me->_dma_ch].out.link.start = 1;\n"
    "\n"
    "    // bool need_yield"
)
REPLACEMENT3 = (
    "      GDMA.channel[me->_dma_ch].out.link.start = 1;\n"
    "      lgfx_vsync_callback(); // patched: signal frame start for VSYNC-synced rendering\n"
    "\n"
    "    // bool need_yield"
)

if not os.path.isfile(bus_rgb_path):
    print("patch_lgfx.py: Bus_RGB.cpp not found, skipping")
else:
    with open(bus_rgb_path, "r", encoding="utf-8") as fh:
        content = fh.read()

    modified = False

    # Patch 1
    if NEEDLE1 in content:
        print("patch_lgfx.py: patching Bus_RGB.cpp — adding ESP_INTR_FLAG_SHARED to LCD_CAM isr_flags")
        content = content.replace(NEEDLE1, REPLACEMENT1, 1)
        modified = True

    # Patches 2a + 2b — applied together, skipped if already present
    if "lgfx_vsync_callback" not in content:
        if NEEDLE2 in content:
            print("patch_lgfx.py: patching Bus_RGB.cpp — adding VSYNC callback weak declaration")
            content = content.replace(NEEDLE2, REPLACEMENT2, 1)
            modified = True
        if NEEDLE3 in content:
            print("patch_lgfx.py: patching Bus_RGB.cpp — adding lgfx_vsync_callback() call in VSYNC_END ISR")
            content = content.replace(NEEDLE3, REPLACEMENT3, 1)
            modified = True

    if modified:
        with open(bus_rgb_path, "w", encoding="utf-8") as fh:
            fh.write(content)
