"""
patch_lgfx.py — PlatformIO extra_script (pre:)

Patch 2 — VSYNC callback hook:
  The LCD_CAM VSYNC_END interrupt fires when GDMA restarts scanning from the
  top of the framebuffer (~every 16.7 ms at 60 Hz).  Adding a weak
  lgfx_vsync_callback() call there lets DisplayManager::loop() synchronise
  lv_timer_handler() to this event: rendering starts right after the GDMA
  restarts, giving the memcpy in the flush callback a full frame budget before
  the GDMA reaches the dirty UI region.

  LovyanGFX >= 1.2.21 already hardcodes ESP_INTR_FLAG_SHARED in the LCD_CAM
  interrupt allocation, so no interrupt-flag patch is needed.

Patch 3 — GDMA initial start address fix:
  Bus_RGB::init() sets the initial GDMA outlink address to (uintptr_t)&(_dmadesc),
  which is the address OF THE POINTER VARIABLE (dma_descriptor_t**), not the
  address OF THE FIRST DESCRIPTOR (dma_descriptor_t*).  The GDMA reads this as
  the first descriptor's dw0 field, gets a raw heap pointer value (e.g.
  0x3FC84000) with size=0, and stops immediately.  Since the GDMA never runs,
  the LCD_CAM VSYNC interrupt never fires, and the screen stays permanently black.

  Fix: change &(_dmadesc) → (_dmadesc) so GDMA starts from the real first
  descriptor.  The circular descriptor chain then loops the framebuffer scan
  indefinitely without needing the VSYNC ISR to restart it each frame.

This patch is idempotent.
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

# ---------------------------------------------------------------------------
# Patch 3 — fix initial GDMA outlink address: &(_dmadesc) → (_dmadesc)
# The init code uses &_dmadesc (address of the pointer variable) instead of
# _dmadesc (address of the first descriptor).  GDMA reads garbage, stops
# immediately, VSYNC interrupt never fires, screen stays black.
# ---------------------------------------------------------------------------
NEEDLE4      = (
    "    GDMA.channel[_dma_ch].out.link.addr = (uintptr_t)&(_dmadesc);\n"
    "    GDMA.channel[_dma_ch].out.link.start = 1;\n"
    "    //////////////////////////////////////////////\n"
)
REPLACEMENT4 = (
    "    GDMA.channel[_dma_ch].out.link.addr = (uintptr_t)(_dmadesc);  // patched: descriptor address, not pointer-to-pointer\n"
    "    GDMA.channel[_dma_ch].out.link.start = 1;\n"
    "    //////////////////////////////////////////////\n"
)

# ---------------------------------------------------------------------------
# Patch 4 — fix VSYNC ISR: add ESP_INTR_FLAG_SHARED to isr_flags
# esp_lcd_new_i80_bus() claims the LCD_CAM IRQ without SHARED.  Without SHARED
# the second esp_intr_alloc_intrstatus call fails silently, _intr_handle stays
# null, esp_intr_enable is a no-op → VSYNC ISR never fires → rolling display.
# Also adds error logging so any future failure is visible in the monitor.
# ---------------------------------------------------------------------------
NEEDLE5      = (
    "    int isr_flags = LGFX_INTR_FLAGS; // ESP_INTR_FLAG_INTRDISABLED | ESP_INTR_FLAG_SHARED;\n"
    "\n"
    "#if SOC_LCDCAM_RGB_LCD_SUPPORTED\n"
    "  auto sigs = LGFX_LCD_RGB_SIG(_cfg.port);\n"
    "#else\n"
    "  auto sigs = LGFX_LCD_I80_SIG(_cfg.port);\n"
    "#endif\n"
    "\n"
    "    esp_intr_alloc_intrstatus(sigs->irq_id, isr_flags,\n"
    "                                   (uint32_t)&dev->lc_dma_int_st,\n"
    "                                    LCD_LL_EVENT_VSYNC_END, lcd_default_isr_handler, this, &_intr_handle);\n"
    "    esp_intr_enable(_intr_handle);\n"
)
REPLACEMENT5 = (
    "    // patched: force SHARED — esp_lcd_new_i80_bus() claims the LCD_CAM IRQ;\n"
    "    // without SHARED, esp_intr_alloc_intrstatus fails silently (_intr_handle=null)\n"
    "    // → esp_intr_enable(null) is a no-op → VSYNC ISR never fires → rolling display.\n"
    "    int isr_flags = LGFX_INTR_FLAGS | ESP_INTR_FLAG_SHARED;\n"
    "\n"
    "#if SOC_LCDCAM_RGB_LCD_SUPPORTED\n"
    "  auto sigs = LGFX_LCD_RGB_SIG(_cfg.port);\n"
    "#else\n"
    "  auto sigs = LGFX_LCD_I80_SIG(_cfg.port);\n"
    "#endif\n"
    "\n"
    "    {\n"
    "        esp_err_t rc = esp_intr_alloc_intrstatus(sigs->irq_id, isr_flags,\n"
    "                                       (uint32_t)&dev->lc_dma_int_st,\n"
    "                                        LCD_LL_EVENT_VSYNC_END, lcd_default_isr_handler, this, &_intr_handle);\n"
    "        if (rc != ESP_OK) {\n"
    "            ESP_LOGE(\"Bus_RGB\", \"esp_intr_alloc_intrstatus failed: %d (handle=%p)\", rc, (void*)_intr_handle);\n"
    "        } else {\n"
    "            esp_intr_enable(_intr_handle);\n"
    "        }\n"
    "    }\n"
)

if not os.path.isfile(bus_rgb_path):
    print("patch_lgfx.py: Bus_RGB.cpp not found, skipping")
else:
    with open(bus_rgb_path, "r", encoding="utf-8") as fh:
        content = fh.read()

    modified = False

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

    # Patch 3 — fix initial GDMA descriptor address
    if NEEDLE4 in content:
        print("patch_lgfx.py: patching Bus_RGB.cpp — fixing initial GDMA outlink address (&_dmadesc → _dmadesc)")
        content = content.replace(NEEDLE4, REPLACEMENT4, 1)
        modified = True

    # Patch 4 — add ESP_INTR_FLAG_SHARED to VSYNC interrupt allocation
    if NEEDLE5 in content:
        print("patch_lgfx.py: patching Bus_RGB.cpp — adding ESP_INTR_FLAG_SHARED to VSYNC interrupt flags")
        content = content.replace(NEEDLE5, REPLACEMENT5, 1)
        modified = True

    if modified:
        with open(bus_rgb_path, "w", encoding="utf-8") as fh:
            fh.write(content)

# ===========================================================================
# Bus_RGB.hpp — double-buffering fields and API (Patches 5 + 6)
# ===========================================================================
bus_rgb_hpp_path = os.path.join(
    env.subst("$PROJECT_DIR"),
    ".pio", "libdeps",
    env.subst("$PIOENV"),
    "LovyanGFX", "src", "lgfx", "v1", "platforms", "esp32s3", "Bus_RGB.hpp",
)

# Patch 5 — insert getBackBuffer() / requestSwap() into the public section
# Needle: the last public method just before "private:"
NEEDLE_HPP5 = "  private:\n    config_t _cfg;\n"
REPLACEMENT_HPP5 = (
    "    // patched: double-buffering API\n"
    "    /// Returns the buffer NOT currently scanned by GDMA (the back buffer).\n"
    "    uint8_t* getBackBuffer() const {\n"
    "        if (_frame_buffer2 == nullptr) return _frame_buffer;\n"
    "        return _buf2_active ? _frame_buffer : _frame_buffer2;\n"
    "    }\n"
    "    /// Requests GDMA to switch to the back buffer at the next VSYNC_END.\n"
    "    void requestSwap() { _swap_pending = true; }\n"
    "\n"
    "  private:\n"
    "    config_t _cfg;\n"
)

# Patch 6 — add new private fields after _frame_buffer
NEEDLE_HPP6 = (
    "    uint8_t *_frame_buffer = nullptr;\n"
    "    intr_handle_t _intr_handle;\n"
)
REPLACEMENT_HPP6 = (
    "    uint8_t *_frame_buffer = nullptr;\n"
    "    // patched: second framebuffer for double-buffering\n"
    "    dma_descriptor_t  _dmadesc_restart2 = {};\n"
    "    dma_descriptor_t* _dmadesc2         = nullptr;\n"
    "    uint8_t*          _frame_buffer2    = nullptr;\n"
    "    volatile bool     _buf2_active      = false;  ///< true = _frame_buffer2 is the GDMA front buffer\n"
    "    volatile bool     _swap_pending     = false;  ///< true = swap requested for next VSYNC_END\n"
    "    intr_handle_t _intr_handle;\n"
)

if not os.path.isfile(bus_rgb_hpp_path):
    print("patch_lgfx.py: Bus_RGB.hpp not found, skipping HPP patches")
else:
    with open(bus_rgb_hpp_path, "r", encoding="utf-8") as fh:
        hpp_content = fh.read()

    hpp_modified = False

    if "patched: double-buffering API" not in hpp_content:
        if NEEDLE_HPP5 in hpp_content:
            print("patch_lgfx.py: patching Bus_RGB.hpp — adding getBackBuffer()/requestSwap() (Patch 5)")
            hpp_content = hpp_content.replace(NEEDLE_HPP5, REPLACEMENT_HPP5, 1)
            hpp_modified = True
        else:
            print("patch_lgfx.py: WARNING — Patch 5 needle not found in Bus_RGB.hpp")

    if "patched: second framebuffer for double-buffering" not in hpp_content:
        if NEEDLE_HPP6 in hpp_content:
            print("patch_lgfx.py: patching Bus_RGB.hpp — adding double-buffer private fields (Patch 6)")
            hpp_content = hpp_content.replace(NEEDLE_HPP6, REPLACEMENT_HPP6, 1)
            hpp_modified = True
        else:
            print("patch_lgfx.py: WARNING — Patch 6 needle not found in Bus_RGB.hpp")

    if hpp_modified:
        with open(bus_rgb_hpp_path, "w", encoding="utf-8") as fh:
            fh.write(hpp_content)

# ===========================================================================
# Bus_RGB.cpp — second buffer allocation (Patch 7) + ISR double-buffer swap (Patch 8)
# ===========================================================================
if os.path.isfile(bus_rgb_path):
    with open(bus_rgb_path, "r", encoding="utf-8") as fh:
        content = fh.read()

    modified = False

    # Patch 7 — allocate second framebuffer and build its descriptor chain
    NEEDLE7 = (
        "    _dmadesc_restart.dw0.length -= skip_bytes;\n"
        "    _dmadesc_restart.dw0.size -= skip_bytes;\n"
        "\n"
        "\n"
        "    uint32_t hsw = _cfg.hsync_pulse_width;\n"
    )
    REPLACEMENT7 = (
        "    _dmadesc_restart.dw0.length -= skip_bytes;\n"
        "    _dmadesc_restart.dw0.size -= skip_bytes;\n"
        "\n"
        "    // patched: allocate second framebuffer for double-buffering\n"
        "    {\n"
        "        auto data2 = (uint8_t*)heap_alloc_psram(fb_len);\n"
        "        _frame_buffer2 = data2;\n"
        "        if (data2) {\n"
        "            size_t dmadesc_size2 = (fb_len - 1) / MAX_DMA_LEN + 1;\n"
        "            auto d2 = (dma_descriptor_t*)heap_caps_malloc(sizeof(dma_descriptor_t) * dmadesc_size2, MALLOC_CAP_DMA);\n"
        "            _dmadesc2 = d2;\n"
        "            size_t len2 = fb_len;\n"
        "            while (len2 > MAX_DMA_LEN) {\n"
        "                len2 -= MAX_DMA_LEN;\n"
        "                d2->buffer = (uint8_t*)data2;\n"
        "                data2 += MAX_DMA_LEN;\n"
        "                *(uint32_t*)d2 = MAX_DMA_LEN | MAX_DMA_LEN << 12 | 0x80000000;\n"
        "                d2->next = d2 + 1;\n"
        "                d2++;\n"
        "            }\n"
        "            *(uint32_t*)d2 = ((len2 + 3) & (~3)) | len2 << 12 | 0xC0000000;\n"
        "            d2->buffer = (uint8_t*)data2;\n"
        "            d2->next = _dmadesc2;\n"
        "            memcpy(&_dmadesc_restart2, _dmadesc2, sizeof(_dmadesc_restart2));\n"
        "            auto p2 = (uint8_t*)(_dmadesc_restart2.buffer);\n"
        "            _dmadesc_restart2.buffer = &p2[skip_bytes];\n"
        "            _dmadesc_restart2.dw0.length -= skip_bytes;\n"
        "            _dmadesc_restart2.dw0.size -= skip_bytes;\n"
        "        }\n"
        "    }\n"
        "\n"
        "\n"
        "    uint32_t hsw = _cfg.hsync_pulse_width;\n"
    )

    # Patch 8 — replace fixed GDMA restart descriptor with double-buffer-aware swap
    NEEDLE8 = (
        "      GDMA.channel[me->_dma_ch].out.link.addr = (uintptr_t)&(me->_dmadesc_restart);\n"
        "      GDMA.channel[me->_dma_ch].out.link.start = 1;\n"
        "      lgfx_vsync_callback(); // patched: signal frame start for VSYNC-synced rendering\n"
    )
    REPLACEMENT8 = (
        "      // patched: double-buffering — apply pending buffer swap before GDMA restart\n"
        "      if (me->_swap_pending && me->_frame_buffer2 != nullptr) {\n"
        "          me->_swap_pending = false;\n"
        "          me->_buf2_active = !me->_buf2_active;\n"
        "      }\n"
        "      dma_descriptor_t* const restart = me->_buf2_active\n"
        "          ? &(me->_dmadesc_restart2) : &(me->_dmadesc_restart);\n"
        "      GDMA.channel[me->_dma_ch].out.link.addr = (uintptr_t)restart;\n"
        "      GDMA.channel[me->_dma_ch].out.link.start = 1;\n"
        "      lgfx_vsync_callback(); // patched: signal frame start for VSYNC-synced rendering\n"
    )

    if "patched: allocate second framebuffer for double-buffering" not in content:
        if NEEDLE7 in content:
            print("patch_lgfx.py: patching Bus_RGB.cpp — allocating second framebuffer (Patch 7)")
            content = content.replace(NEEDLE7, REPLACEMENT7, 1)
            modified = True
        else:
            print("patch_lgfx.py: WARNING — Patch 7 needle not found in Bus_RGB.cpp")

    if "patched: double-buffering — apply pending buffer swap" not in content:
        if NEEDLE8 in content:
            print("patch_lgfx.py: patching Bus_RGB.cpp — double-buffer ISR swap (Patch 8)")
            content = content.replace(NEEDLE8, REPLACEMENT8, 1)
            modified = True
        else:
            print("patch_lgfx.py: WARNING — Patch 8 needle not found in Bus_RGB.cpp")

    if modified:
        with open(bus_rgb_path, "w", encoding="utf-8") as fh:
            fh.write(content)
