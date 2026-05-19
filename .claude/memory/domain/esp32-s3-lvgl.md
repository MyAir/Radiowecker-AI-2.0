# Domain: ESP32-S3 + LVGL 9 + LovyanGFX

## 2026-05-19 — Display oscillation fix: LVGL DIRECT mode + both PSRAM framebuffers

**Symptom**: display oscillated between two visibly different states (full UI
vs. only the periodically-updated regions on a stale background).

**Root cause**: hardware double-buffering (Bus_RGB patches 5–8) is incompatible
with `LV_DISPLAY_RENDER_MODE_PARTIAL`. LVGL only flushes dirty rectangles, but
each flush lands in whichever PSRAM framebuffer is currently the back. Regions
never re-rendered (drawn once by `create()`) exist in only one of the two
buffers → GDMA alternates them → user sees flicker.

**Fix** (`DisplayManager.cpp` + `lgfx_config.h`): switched LVGL to
`LV_DISPLAY_RENDER_MODE_DIRECT` and hand BOTH PSRAM framebuffers to
`lv_display_set_buffers(..., fb_back_initial, fb_front_initial, 768000,
LV_DISPLAY_RENDER_MODE_DIRECT)`. In DIRECT mode LVGL natively re-renders the
previous frame's dirty rectangles into the now-current buffer, so both buffers
stay coherent without any manual front→back copy. `_lvglFlush` just byte-swaps
dirty rows in place, `Cache_WriteBack_Addr`s them, and on
`lv_display_flush_is_last()` calls `_gfx.requestSwap()`. Render task: bare
`xSemaphoreTake(vsync_sem) → lv_timer_handler()` loop. All the previous
front→back row-copy + `s_prev_y1/y2` bookkeeping was deleted.

**Footgun**: `Bus_RGB::getDMABuffer(0)` always returns `_frame_buffer`
regardless of swap state — must use patched `getFrontBuffer()` /
`getBackBuffer()` to know the actually scanned buffer. Initial GDMA state
after `_gfx.init()`: `_buf2_active=false`, GDMA scans `_frame_buffer`, so
`getFrontBuffer()==_frame_buffer` and `getBackBuffer()==_frame_buffer2`.

**Earlier attempt (SUPERSEDED)**: tracked previous-frame dirty y-range and
copied front→back rows at start of next render iteration. Made the screen
worse (likely PSRAM bus contention from copy reads of the buffer GDMA was
scanning, plus race on `_buf2_active` between separate getBack/getFront
reads). DIRECT mode is the correct architecture.

## 2026-05-19 — I2C_NUM_1 driver conflict — RESOLVED via Option 1

**Was**: `[E][esp32-hal-i2c-ng.c:275] i2cWrite(): i2c_master_transmit failed:
[259] ESP_ERR_INVALID_STATE` every few seconds. LovyanGFX `Touch_GT911`
installed the legacy ESP-IDF i2c driver on I2C_NUM_1 during `_gfx.init()`;
later `Wire1.begin()` in `SensorManager` installed arduino-esp32 3.x
`i2c-ng` driver on the same port → drivers fought for the hardware.

**Fix (Option 1)**: removed `Touch_GT911` entirely from `lgfx_config.h` (no
`_touch_instance`, no touch constructor block). `DisplayManager::pollTouch()`
now talks to GT911 manually over `Wire1` (the same bus the sensors use):
- `_initGT911()`: pulse `TOUCH_RST_PIN` LOW 10 ms then HIGH 60 ms after
  `_gfx.init()`. `Wire1.begin(SDA, SCL, 100 kHz)` happens later in
  `SensorManager::begin()` — single driver on the port.
- `pollTouch()`: read status reg 0x814E (bit7=ready, bits3:0=count); if a
  point is present read 7 B at 0x8150, parse x_lo/hi y_lo/hi → cache
  `s_touch_x/y/pressed`; write 0 to 0x814E to clear.
- `_lvglTouch`: reads cached `s_touch_*` only (no I2C in LVGL callback).

**Rule**: an `I2C_NUM_x` port can host only ONE driver at a time — never mix
the legacy ESP-IDF `driver/i2c.h` driver with arduino-esp32 3.x `i2c-ng`
(`Wire`) on the same port.

## 2026-05-19 — USB-CDC Serial: bump TX buffer to stop first-char drops

**Symptom**: PIO monitor occasionally drops the first 1–4 chars of a line
(`[Sensors]` → `Sensors]`, `[Display]` → `lay]`).

**Cause**: default arduino-esp32 USB-CDC TX buffer (256 B) overruns when
multiple tasks (render task on Core 1, main-loop sensor prints, network/WiFi
logs) print at the same time while the host (pio monitor) momentarily stops
draining. On overrun the leading bytes of the next write get dropped.

**Fix** (`main.cpp` setup): `Serial.setTxBufferSize(2048);` BEFORE
`Serial.begin(115200);` (must precede `begin`). If still seen under heavy
load, next step would be a serial mutex around all `Serial.printf` callers,
or move the render-task VSYNC log to a counter consumed by the main loop.

## 2026-05-15 — Board: Makerfabs MaTouch ESP32-S3 Parallel TFT 4.3" V3.1

**MCU**: ESP32-S3-WROOM-1-N16R8 — 16 MB Flash, 8 MB OPI PSRAM  
**Board**: `esp32-s3-devkitc-1` in PlatformIO, `board_build.arduino.memory_type = qio_opi`  
**Wireless**: WiFi + Bluetooth 5.0  
**USB**: Dual USB-C — native USB (USB OTG) + USB-to-UART via CP2104  
**Power**: 5 V USB-C (4.0 V–5.25 V)  
**Mabee**: 1× I2C, 1× GPIO connectors  
**Links**: [GitHub](https://github.com/Makerfabs/ESP32-S3-Parallel-TFT-with-Touch-4.3inch/tree/main) · [Hardware diagrams](https://github.com/Makerfabs/ESP32-S3-Parallel-TFT-with-Touch-4.3inch/tree/main/hardware) · [Wiki](https://wiki.makerfabs.com/MaTouch_S3_Parallel_4.3_TFT_with_Touch.html)

### Display — ST7262 RGB565 800×480
| Signal | GPIO |
|--------|------|
| PCLK | 42 |
| VSYNC | 41 |
| HSYNC | 39 |
| DE | 40 |
| B0–B4 | 8, 3, 46, 9, 1 |
| G0–G5 | 5, 6, 7, 15, 16, 4 |
| R0–R4 | 45, 48, 47, 21, 14 |
| Backlight | 44 (PWM, **inverted** — R29 mod; low = full bright) |

Timing (confirmed working, reverted to commit 8c930a0): `freq_write=16MHz, hsync_front/back_porch=8, hsync_pulse_width=4, vsync_front/back_porch=8, vsync_pulse_width=4, pclk_idle_high=1`

### Touch — GT911 (I2C, polling)
- I2C_NUM_1, SDA=17, SCL=18, RST=38, INT=NC (−1)
- I2C addr 0x5D (default when INT floats); alt 0x14

### I2C Bus (shared: touch + sensors)
- SDA=17, SCL=18, 100 kHz (Wire1)
- Touch uses 400 kHz internally via LovyanGFX config

### SD Card (SPI)
- CS=10, MOSI=11, SCK=12, MISO=13

### I2S Audio
- BCLK=20, LRCLK=19, DOUT=2
- ⚠️ GPIO 19/20 = USB D±; disable USB CDC if I2S is used here

### Sensors (Mabee I2C connector → Wire1)
- SGP30 TVOC/eCO2: addr 0x58 (fixed)
- SHT31 Temp/Humidity: addr 0x44 (ADDR low); alt 0x45
- Mabee GPIO light sensor: `LIGHT_SENSOR_PIN = -1` (disabled)
  - GPIO 22 is **NOT** ADC-capable on ESP32-S3 (ADC1 = GPIO 1–10, ADC2 = GPIO 11–20)
  - All ADC-capable GPIOs are occupied by display lines, SD, I2C, I2S
  - Guard all ADC calls with `if (LIGHT_SENSOR_PIN >= 0)` — returns 0 when disabled

---

## 2026-05-15 — LovyanGFX 1.2.21 Config Pattern (CORRECTED)

**Must** include platform-specific headers — `Panel_RGB`/`Bus_RGB` are NOT in `<LovyanGFX.hpp>`:
```cpp
#define LGFX_USE_V1
#include <LovyanGFX.hpp>
#include <lgfx/v1/platforms/esp32s3/Panel_RGB.hpp>
#include <lgfx/v1/platforms/esp32s3/Bus_RGB.hpp>

class LGFX : public lgfx::LGFX_Device {
    lgfx::Panel_RGB   _panel_instance;   // ⚠ NOT _panel — LGFXBase has IPanel* _panel!
    lgfx::Bus_RGB     _bus_instance;
    lgfx::Touch_GT911 _touch_instance;
    lgfx::Light_PWM   _light_instance;
public:
    LGFX() {
        { auto cfg = _panel_instance.config(); /* w/h, offsets */ _panel_instance.config(cfg); }
        { auto cfg = _panel_instance.config_detail(); cfg.use_psram = 1; _panel_instance.config_detail(cfg); }
        { auto cfg = _bus_instance.config(); cfg.panel = &_panel_instance; /* pins, timing */ _bus_instance.config(cfg); }
        _panel_instance.setBus(&_bus_instance);
        { auto cfg = _touch_instance.config(); /* pins, i2c */ _touch_instance.config(cfg); _panel_instance.setTouch(&_touch_instance); }
        { auto cfg = _light_instance.config(); /* pin, invert */ _light_instance.config(cfg); _panel_instance.light(&_light_instance); } // .light() not .setLight()
        setPanel(&_panel_instance);
    }
};
```
- `lib_archive = no` required in `platformio.ini`
- Reference: `.pio/libdeps/matouch43/LovyanGFX/src/lgfx_user/LGFX_ESP32S3_RGB_MakerfabsParallelTFTwithTouch43.h`

---

## 2026-05-15 — LVGL 9.2.2 Integration

> ⚠️ Tick and loop patterns below are **WRONG for LVGL 9.2.2** — see 2026-05-18 for the correct final setup.

- `lv_conf.h` in `include/`; build flag `-DLV_CONF_INCLUDE_SIMPLE`
- ~~Tick: `LV_TICK_CUSTOM 1` → `millis()`~~ — LVGL 8 API; **ignored** by LVGL 9.2.2. Use `lv_tick_set_cb()`.
- Memory: `LV_MEM_CUSTOM 1` → `malloc/free` (uses PSRAM via `heap_caps_malloc`)
- Draw buffers: allocate in PSRAM with `heap_caps_malloc(…, MALLOC_CAP_SPIRAM)`
- ~~Call `lv_timer_handler_run_in_period(5)`~~ — superseded by VSYNC-gated `lv_timer_handler()`

**LVGL 9 callback signatures (still valid):**
```cpp
// Flush:
static void flush_cb(lv_display_t *display, const lv_area_t *area, uint8_t *px_map);
// call lv_display_flush_ready(display) when done

// Touch:
static void touch_cb(lv_indev_t *indev, lv_indev_data_t *data);
// data->state = LV_INDEV_STATE_PRESSED / RELEASED
// data->point.x / .y = int32_t
```

---

## 2026-05-15 — Partition Layout (16 MB)

```csv
nvs,      data, nvs,      0x9000,   0x5000
otadata,  data, ota,      0xe000,   0x2000
app0,     app,  ota_0,    0x10000,  0x600000   (6 MB)
app1,     app,  ota_1,    0x610000, 0x600000   (6 MB)
littlefs, data, spiffs,   0xC10000, 0x3E0000   (3.9 MB)
coredump, data, coredump, 0xFF0000, 0x10000
```

---

## 2026-05-16 — LVGL Flush Callback: CONFIRMED WORKING Pattern (2026-05-16)

> ⚠️ **SUPERSEDED by 2026-05-18 entry.** `pushImage` works but bypasses GDMA and has tearing. VSYNC-gated direct GDMA write is the correct final solution.

**Use `pushImage` + call `lv_refr_now()` every loop.**

```cpp
// Flush callback — correct, confirmed working:
void DisplayManager::_lvglFlush(lv_display_t *display,
                                  const lv_area_t *area, uint8_t *px_map) {
    auto *dm = static_cast<DisplayManager*>(lv_display_get_user_data(display));
    const int32_t w = area->x2 - area->x1 + 1;
    const int32_t h = area->y2 - area->y1 + 1;
    dm->_gfx.pushImage(area->x1, area->y1, w, h,
                       reinterpret_cast<lgfx::rgb565_t*>(px_map));
    lv_display_flush_ready(display);
}

// loop() — must call both:
void DisplayManager::loop() {
    lv_timer_handler_run_in_period(5);  // timers, animations
    lv_refr_now(s_display);             // bypass timer-pause; no-op if no dirty areas
}
```

**Why `lv_refr_now()` is needed:** LVGL 9's refr timer pauses itself after each fire (`lv_timer_pause(tmr)` inside `lv_display_refr_timer`). It only resumes on `LV_EVENT_REFR_REQUEST`. If that event chain is delayed or missed (e.g. WiFi blocking in `setup()`), the timer stays paused and the screen stays black. `lv_refr_now()` bypasses the timer system entirely.

**Previous approaches that were BLACK (without `lv_refr_now`):**
- `startWrite + setAddrWindow + writePixels + endWrite` → black (timer issue, not flush issue)
- `pushImage` alone (without `lv_refr_now` in loop) → black for same reason
- Both approaches would have worked IF `lv_refr_now` was also called in loop

---

## 2026-05-18 — FINAL CONFIRMED WORKING Solution (all display issues resolved)

### Summary of all issues fixed
| # | Symptom | Root cause | Fix |
|---|---------|-----------|-----|
| 1 | Wrong colors | LVGL writes native rgb565, LGFX GDMA fb uses swap565 | `lv_draw_sw_rgb565_swap()` in flush |
| 2 | `esp_cache_msync` crash | Address/size not 32-byte aligned | Round down start, round up end to 32-byte boundaries |
| 3 | 2-state oscillation | DIRECT mode swapped bytes in-place; LVGL re-read swapped bytes | Switch to PARTIAL mode with separate PSRAM render buffer |
| 4 | DMA tearing | `lv_timer_handler` called at 200 Hz, mid-frame | Gate `lv_timer_handler()` on VSYNC only (~60 Hz) |
| 5 | No updates (screen static) | `LV_TICK_CUSTOM` is LVGL 8 legacy, ignored by LVGL 9.2.2 | `lv_tick_set_cb(millis)` after `lv_init()` |
| 6 | Garbled/switching pixels | `LV_DRAW_BUF_STRIDE_ALIGN 4` padded odd-width rows; flush assumed `stride=w*2` | Remove override; keep default `LV_DRAW_BUF_STRIDE_ALIGN 1` |

### LVGL 9 Tick (critical — LVGL 8 API does NOT work)
```cpp
// WRONG (LVGL 8 — ignored by LVGL 9.2.2):
// lv_conf.h: LV_TICK_CUSTOM 1, LV_TICK_CUSTOM_INCLUDE "Arduino.h", LV_TICK_CUSTOM_SYS_TIME_EXPR millis()
// If tick stays 0: all timers fire once then never fire again.

// CORRECT (LVGL 9):
lv_init();
lv_tick_set_cb([]() -> uint32_t { return (uint32_t)millis(); });
```

### VSYNC-gated rendering
LovyanGFX `Bus_RGB.cpp` is patched by `scripts/patch_lgfx.py` to call `lgfx_vsync_callback()`
from the VSYNC_END ISR. The ISR gives a binary semaphore; `loop()` takes it (non-blocking) and
calls `lv_timer_handler()` once per frame.

```cpp
// DisplayManager.cpp — loop:
void DisplayManager::loop() {
    if (s_vsync_sem && xSemaphoreTake(s_vsync_sem, 0) == pdTRUE) {
        lv_timer_handler();
    }
}

// ISR (defined in DisplayManager.cpp, called from patched Bus_RGB):
extern "C" void IRAM_ATTR lgfx_vsync_callback() {
    if (s_vsync_sem) {
        BaseType_t hp = pdFALSE;
        xSemaphoreGiveFromISR(s_vsync_sem, &hp);
        portYIELD_FROM_ISR(hp);
    }
}
```

### Flush callback (GDMA direct write — final, confirmed working)
- PSRAM render buffer: 40 lines × 800 px × 2 bytes = 64 KB
- `use_psram=1` in LGFX config → `Bus_RGB` allocates one 768 KB PSRAM framebuffer (`_frame_buffer`)
- `getFrameBuffer()` wraps `_bus_instance.getDMABuffer(0)` (added to `lgfx_config.h`)
- GDMA pixel format: `swap565_t` (byte[0]=RRRRRGGG, byte[1]=GGGBBBBB); LVGL writes native rgb565 → must swap

```cpp
void DisplayManager::_lvglFlush(lv_display_t *display,
                                  const lv_area_t *area, uint8_t *px_map) {
    const int32_t area_w      = area->x2 - area->x1 + 1;
    const int32_t dirty_lines = area->y2 - area->y1 + 1;
    const size_t  area_px     = (size_t)area_w * dirty_lines;

    lv_draw_sw_rgb565_swap(px_map, area_px);  // swap bytes in render buf

    uint8_t* dst = s_gdma_fb
                 + (size_t)area->y1 * TFT_WIDTH * 2
                 + (size_t)area->x1 * 2;
    const uint8_t* src = px_map;
    const size_t row_bytes = (size_t)area_w * 2;
    for (int32_t y = 0; y < dirty_lines; y++) {
        memcpy(dst, src, row_bytes);
        dst += TFT_WIDTH * 2;
        src += row_bytes;      // assumes LV_DRAW_BUF_STRIDE_ALIGN == 1 (default)
    }

    // Write-back cache to PSRAM (must be 32-byte aligned)
    uint8_t* const fb_row = s_gdma_fb + (size_t)area->y1 * TFT_WIDTH * 2;
    const size_t wb_bytes = (size_t)dirty_lines * TFT_WIDTH * 2;
    const uintptr_t start = (uintptr_t)fb_row & ~(uintptr_t)31;
    const uintptr_t end   = ((uintptr_t)fb_row + wb_bytes + 31) & ~(uintptr_t)31;
    esp_cache_msync((void*)start, end - start,
                    ESP_CACHE_MSYNC_FLAG_DIR_C2M | ESP_CACHE_MSYNC_FLAG_TYPE_DATA);
    lv_display_flush_ready(display);
}
```

### lv_conf.h critical settings
```c
#define LV_DRAW_BUF_ALIGN        4   // buffer start alignment — OK
#define LV_DRAW_BUF_STRIDE_ALIGN 1   // row stride alignment — MUST be 1 (default)
// ⚠️ DO NOT set LV_DRAW_BUF_STRIDE_ALIGN to anything > 1.
// LVGL pads render-buffer row stride to LV_ROUND_UP(w*2, STRIDE_ALIGN).
// Flush callback assumes stride = area_w*2 exactly.
// With STRIDE_ALIGN=4, odd-width dirty areas get 2 bytes of padding per row
// → flush copies wrong pixels for rows 2,3,…  → garbled/tilted pixels.
// Clock digit widths vary (proportional font) → some time values fine, others garbled.
```

### Display init sequence (begin())
```cpp
lv_init();
lv_tick_set_cb([]() -> uint32_t { return (uint32_t)millis(); });
s_gdma_fb = _gfx.getFrameBuffer();       // pointer to PSRAM GDMA framebuffer
// alloc 40-line PSRAM render buffer
s_vsync_sem = xSemaphoreCreateBinary();
s_display = lv_display_create(TFT_WIDTH, TFT_HEIGHT);
lv_display_set_buffers(s_display, s_render_buf, nullptr, render_size,
                       LV_DISPLAY_RENDER_MODE_PARTIAL);
lv_display_set_flush_cb(s_display, _lvglFlush);
```

**Do NOT set `LV_COLOR_16_SWAP 1`** — causes color corruption. Keep it 0 (unset).

---

## 2026-05-16 — LittleFS Partition Label Bug

```cpp
// WRONG — looks for partition labeled "spiffs" (default):
LittleFS.begin(true)

// CORRECT — partition in partitions.csv is named "littlefs":
LittleFS.begin(true, "/littlefs", 10, "littlefs")
```
partitions.csv has: `littlefs, data, spiffs, 0xC10000, 0x3E0000` — name="littlefs", subtype=spiffs.
`LittleFS.begin()` searches by partition **name**, not subtype.

---

## 2026-05-16 — LVGL Fallback Buffer Size Bug

If PSRAM malloc fails, fallback must pass the **actual** buffer size to `lv_display_set_buffers`, not the PSRAM buffer size. Old bug: allocated 16 KB but passed `BUF_SIZE=80 KB` → silent heap overflow when LVGL rendered.

```cpp
// Correct fallback pattern:
static constexpr size_t FALLBACK_LINES = 10;
static constexpr size_t FALLBACK_SIZE  = TFT_WIDTH * FALLBACK_LINES * sizeof(lv_color16_t);
s_buf1 = new lv_color16_t[TFT_WIDTH * FALLBACK_LINES];
s_buf2 = new lv_color16_t[TFT_WIDTH * FALLBACK_LINES];
lv_display_set_buffers(s_display, s_buf1, s_buf2, FALLBACK_SIZE, LV_DISPLAY_RENDER_MODE_PARTIAL);
```

---

## 2026-05-19 — Double-Buffering Fix for "Screen Shifts Right Every Second"

> **Supersedes** the "timed post-render flush" approach. The 2026-05-18 flush callback using `esp_cache_msync` inside `_lvglFlush` is no longer used.

### Root cause of the scramble
- Display runs at **39 Hz** (not 60 Hz): `16 MHz / (820 × 500) = 39 Hz`, frame period = **25.6 ms**
- V-back-porch after VSYNC_END = `8 × 820 / 16 MHz = 410 µs` (GDMA idle window)
- `Cache_WriteBack_Addr` for 76 KB clock dirty region takes **~600 µs** — overflows V-back-porch by 190 µs
- During overflow: GDMA scans rows 0–3 while `Cache_WriteBack_Addr` writes — bus contention stalls GDMA ~225 µs → **~544 px right-shift** per affected row — visible as full-screen scramble via rolling-shutter camera

### Failed approach: timed vTaskDelay
Used wrong frame constant (17 ms instead of 25.6 ms) → `safe_ms` calculation wrong → flush landed inside the clock dirty rows (138–186) → worse scramble.

### Solution: hardware double buffering via Bus_RGB patches
`patch_lgfx.py` now applies **Patches 5–8** (idempotent, in addition to existing 1–4):

| Patch | File | What |
|-------|------|------|
| 5 | Bus_RGB.hpp | Add `getBackBuffer()` + `requestSwap()` public methods |
| 6 | Bus_RGB.hpp | Add private fields: `_dmadesc_restart2`, `_dmadesc2`, `_frame_buffer2`, `_buf2_active`, `_swap_pending` |
| 7 | Bus_RGB.cpp `init()` | Allocate second 768 KB PSRAM framebuffer + build its descriptor chain + `_dmadesc_restart2` |
| 8 | Bus_RGB.cpp ISR | At `VSYNC_END`, if `_swap_pending`: clear it, toggle `_buf2_active`, pick restart descriptor accordingly |

`lgfx_config.h` adds thin wrappers:
```cpp
uint8_t* getBackBuffer() { return _bus_instance.getBackBuffer(); }
void     requestSwap()   { _bus_instance.requestSwap(); }
```

### Updated flush callback (`_lvglFlush`)
**No longer calls `Cache_WriteBack_Addr` / `esp_cache_msync` inside the flush.**  
Instead: memcpy into back buffer + accumulate dirty range in `s_wb_start` / `s_wb_end`.

### Updated `_renderTask` (priority 5, Core 1)
```cpp
void DisplayManager::_renderTask(void* /*arg*/) {
    for (;;) {
        xSemaphoreTake(s_vsync_sem, pdMS_TO_TICKS(50));
        // ISR has applied any pending swap — refresh back-buffer pointer
        s_gdma_fb = s_instance->_gfx.getBackBuffer();
        s_wb_start = UINTPTR_MAX;
        s_wb_end   = 0;
        lv_timer_handler();  // renders into back buffer via _lvglFlush
        if (s_wb_start < s_wb_end) {
            // GDMA reads front buffer (different PSRAM region) → no content corruption
            Cache_WriteBack_Addr((uint32_t)s_wb_start,
                                 (uint32_t)(s_wb_end - s_wb_start));
            s_wb_start = UINTPTR_MAX;
            s_wb_end   = 0;
            s_instance->_gfx.requestSwap();  // swap back→front at next VSYNC_END
        }
    }
}
```

### `begin()` init change
```cpp
s_gdma_fb = _gfx.getBackBuffer();   // was: _gfx.getFrameBuffer()
// fallback if second buffer alloc failed:
if (!s_gdma_fb) s_gdma_fb = _gfx.getFrameBuffer();
```

### Why it works
`Cache_WriteBack_Addr` writes to back buffer; GDMA reads front buffer (different PSRAM addresses).
Bus contention shifts only front-buffer rows currently under scan — those rows contain unchanged,
static background content → shift imperceptible. New content appears one frame (~25 ms) later.

---

## 2026-05-16 — LCD_CAM Interrupt Fix (ESP-IDF 5.4.4+)
