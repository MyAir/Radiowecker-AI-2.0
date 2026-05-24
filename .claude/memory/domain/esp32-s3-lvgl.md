# Domain: ESP32-S3 + LVGL 9 + LovyanGFX

## 2026-05-21 — OTA flash writes cause unavoidable left/right tearing

What — During an ArduinoOTA transfer, the screen shows left/right tearing/
glitches in horizontal bands.

Why — Framebuffer is in PSRAM (`cfg.use_psram = 1` in `lgfx_config.h`) and
LCD_CAM GDMA continuously scans it out at 14 MHz pclk. Every flash write
disables the cache for a short window, which also pauses PSRAM access. The
GDMA can't fetch pixels during that window, so the panel repeats stale data
or shows garbage in a horizontal band. Independent of LVGL repaint rate —
even a frozen screen tears during OTA.

Decision (2026-05-21) — Accept the glitch; do not blank the backlight. OTA
already works and the visual is brief.

Fixes that DON'T help (verified) — LVGL repaint throttling (only repaint on
integer-percent change); deferring `pollTouch()` until after the
`ota.isUpdating()` guard. Both kept in place because they reduce CPU cost,
but they do not address the cache-disable cause.

Fixes that WOULD help if the glitch ever needs to go away — (a) blank the
backlight via `_gfx.setBrightness(0)` in OTA `onStart`, restore in `onEnd`/
`onError`; (b) move framebuffer to internal DRAM (768 KB RGB565 won't fit on
ESP32-S3 — needs lower res or 8-bit colour); (c) bounce-buffer mode in
`esp_lcd_rgb_panel` (LovyanGFX `Bus_RGB` doesn't expose this).

## 2026-05-20 — CURRENT WORKING SOLUTION: full toolchain swap to EEZ stack

The per-second horizontal-shift glitch (every LVGL refresh, ~100 px right
shift) was fixed by aligning the toolchain with the working
Radiowecker_EEZ_AI reference project. All prior software-only attempts on the
arduino-esp32 3.x / LovyanGFX 1.2.21 stack failed. Root cause was the LCD_CAM
driver in that combination on this PSRAM/OPI configuration.

### Toolchain (KEEP THESE PINS)
- `platform = espressif32 @ 5.4.0`
- `framework-arduinoespressif32 @ 2.0.17` (Arduino-ESP32 2.0.17 / ESP-IDF 4.4.7)
  pinned via
  `platform_packages = framework-arduinoespressif32@https://github.com/espressif/arduino-esp32.git#2.0.17`
- `LovyanGFX @ 1.2.7` (works unpatched)
- `lvgl @ 9.2.2`
- `ArduinoJson @ ^7.0.0`
- `esp_cache.h` is NOT in IDF 4.4 — flush must NOT call `esp_cache_msync`
- Audio is stubbed; ESP8266Audio dropped entirely

### platformio.ini essentials
```ini
build_flags =
    -D BOARD_HAS_PSRAM
    -mfix-esp32-psram-cache-issue
    -D ARDUINO_USB_CDC_ON_BOOT=1
    -D ARDUINO_USB_MODE=1
    -D LV_CONF_INCLUDE_SIMPLE
    -D LGFX_USE_V1
board_build.arduino.memory_type = qio_opi
board_build.psram_type          = opi
extra_scripts = pre:scripts/patch_lvgl.py    # only patch_lvgl is active
```
`patch_lgfx.py` and `patch_esp8266audio.py` are NOT applied (their needles
target 1.2.21 / ESP8266Audio).

### Flush callback (PARTIAL mode, EEZ-style)
Two 100×800×2 = 160 KB PSRAM scratch buffers via `ps_malloc`. No GDMA
back/front juggling, no manual cache writeback.
```cpp
void DisplayManager::_lvglFlush(lv_display_t *display,
                                  const lv_area_t *area, uint8_t *px_map) {
    const int32_t w = area->x2 - area->x1 + 1;
    const int32_t h = area->y2 - area->y1 + 1;
    auto &gfx = s_instance->_gfx;
    gfx.startWrite();
    gfx.setAddrWindow(area->x1, area->y1, w, h);
    gfx.writePixels((uint16_t*)px_map, (uint32_t)w * (uint32_t)h);
    gfx.endWrite();
    lv_display_flush_ready(display);
}
```
With `LV_COLOR_DEPTH=16` + `LV_COLOR_16_SWAP=1`, `writePixels` receives bytes
in the panel's native order. (Rule for `writePixels`-style flushes; the
opposite rule applied to direct GDMA framebuffer writes used by previous
attempts.)

### Panel timings (matching EEZ)
`freq_write=14000000, hsync_back_porch=16, hsync_front_porch=8,
hsync_pulse_width=4, vsync_back_porch=4, vsync_front_porch=4,
vsync_pulse_width=4, pclk_idle_high=1, hsync_polarity=0, vsync_polarity=0`

### Lesson
Arduino-ESP32 3.x + LovyanGFX 1.2.21 LCD_CAM has a regression on this
PSRAM/OPI configuration. 2.0.17 + 1.2.7 is the fix — do not upgrade.

---

## 2026-05-19 — I2C_NUM_1 driver conflict — RESOLVED via manual GT911 over Wire1

**Was**: `[E][esp32-hal-i2c-ng.c:275] i2cWrite(): i2c_master_transmit failed:
[259] ESP_ERR_INVALID_STATE` every few seconds. LovyanGFX `Touch_GT911`
installed the legacy ESP-IDF i2c driver on I2C_NUM_1 during `_gfx.init()`;
later `Wire1.begin()` in `SensorManager` installed arduino-esp32's `i2c-ng`
driver on the same port → drivers fought for the hardware.

**Fix (Option 1)**: removed `Touch_GT911` entirely from `lgfx_config.h` (no
`_touch_instance`, no touch constructor block). `DisplayManager::pollTouch()`
talks to GT911 manually over `Wire1` (the same bus the sensors use):
- `_initGT911()`: pulse `TOUCH_RST_PIN` LOW 10 ms then HIGH 60 ms after
  `_gfx.init()`. `Wire1.begin(SDA, SCL, 100 kHz)` happens later in
  `SensorManager::begin()` — single driver on the port.
- `pollTouch()`: read status reg 0x814E (bit7=ready, bits3:0=count); if a
  point is present read 7 B at 0x8150, parse x_lo/hi y_lo/hi → cache
  `s_touch_x/y/pressed`; write 0 to 0x814E to clear.
- `_lvglTouch`: reads cached `s_touch_*` only (no I2C in LVGL callback).

**Rule**: an `I2C_NUM_x` port can host only ONE driver at a time — never mix
the legacy ESP-IDF `driver/i2c.h` driver with arduino-esp32 `i2c-ng`
(`Wire`) on the same port.

---

## 2026-05-19 — USB-CDC Serial: bump TX buffer to stop first-char drops

**Symptom**: PIO monitor occasionally drops the first 1–4 chars of a line
(`[Sensors]` → `Sensors]`, `[Display]` → `lay]`).

**Cause**: default arduino-esp32 USB-CDC TX buffer (256 B) overruns when
multiple tasks print at the same time while the host (pio monitor)
momentarily stops draining. On overrun the leading bytes of the next write
get dropped.

**Fix** (`main.cpp` setup): `Serial.setTxBufferSize(2048);` BEFORE
`Serial.begin(115200);` (must precede `begin`). If still seen under heavy
load, next step: serial mutex around all `Serial.printf` callers.

---

## 2026-05-16 — LVGL Fallback Buffer Size Bug

If PSRAM malloc fails, fallback must pass the **actual** buffer size to
`lv_display_set_buffers`, not the PSRAM buffer size. Old bug: allocated 16 KB
but passed `BUF_SIZE=80 KB` → silent heap overflow when LVGL rendered.

```cpp
static constexpr size_t FALLBACK_LINES = 10;
static constexpr size_t FALLBACK_SIZE  = TFT_WIDTH * FALLBACK_LINES * sizeof(lv_color16_t);
s_buf1 = new lv_color16_t[TFT_WIDTH * FALLBACK_LINES];
s_buf2 = new lv_color16_t[TFT_WIDTH * FALLBACK_LINES];
lv_display_set_buffers(s_display, s_buf1, s_buf2, FALLBACK_SIZE,
                       LV_DISPLAY_RENDER_MODE_PARTIAL);
```

---

## 2026-05-15 — LovyanGFX RGB Config Pattern (general)

**Must** include platform-specific headers — `Panel_RGB`/`Bus_RGB` are NOT in
`<LovyanGFX.hpp>`:
```cpp
#define LGFX_USE_V1
#include <LovyanGFX.hpp>
#include <lgfx/v1/platforms/esp32s3/Panel_RGB.hpp>
#include <lgfx/v1/platforms/esp32s3/Bus_RGB.hpp>

class LGFX : public lgfx::LGFX_Device {
    lgfx::Panel_RGB   _panel_instance;   // ⚠ NOT _panel — LGFXBase has IPanel* _panel!
    lgfx::Bus_RGB     _bus_instance;
    lgfx::Light_PWM   _light_instance;
public:
    LGFX() {
        { auto cfg = _panel_instance.config();        /* w/h, offsets */                _panel_instance.config(cfg); }
        { auto cfg = _panel_instance.config_detail(); cfg.use_psram = 1;                 _panel_instance.config_detail(cfg); }
        { auto cfg = _bus_instance.config();          cfg.panel = &_panel_instance; /* pins, timing */ _bus_instance.config(cfg); }
        _panel_instance.setBus(&_bus_instance);
        { auto cfg = _light_instance.config();        /* pin, invert */                  _light_instance.config(cfg); _panel_instance.light(&_light_instance); }
        setPanel(&_panel_instance);
    }
};
```
- `lib_archive = no` required in `platformio.ini`
- `Touch_GT911` deliberately NOT instantiated here — see 2026-05-19 entry

### LVGL 9 callback signatures
```cpp
static void flush_cb(lv_display_t *display, const lv_area_t *area, uint8_t *px_map);
// call lv_display_flush_ready(display) when done

static void touch_cb(lv_indev_t *indev, lv_indev_data_t *data);
// data->state = LV_INDEV_STATE_PRESSED / RELEASED, data->point.x/.y = int32_t
```

### LVGL 9 tick (CRITICAL — LVGL 8 API does NOT work)
```cpp
// WRONG (LVGL 8, ignored by LVGL 9.2.2): LV_TICK_CUSTOM 1 + millis() in lv_conf.h
lv_init();
lv_tick_set_cb([]() -> uint32_t { return (uint32_t)millis(); });
```

---

## 2026-05-15 — Board: Makerfabs MaTouch ESP32-S3 Parallel TFT 4.3" V3.1

**MCU**: ESP32-S3-WROOM-1-N16R8 — 16 MB Flash, 8 MB OPI PSRAM
**Board**: `esp32-s3-devkitc-1` in PlatformIO, `board_build.arduino.memory_type = qio_opi`
**USB**: Dual USB-C — native USB (USB OTG) + USB-to-UART via CP2104
**Power**: 5 V USB-C
**Links**: [GitHub](https://github.com/Makerfabs/ESP32-S3-Parallel-TFT-with-Touch-4.3inch/tree/main) · [Wiki](https://wiki.makerfabs.com/MaTouch_S3_Parallel_4.3_TFT_with_Touch.html)

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

Current panel timings: see 2026-05-20 entry.

### Touch — GT911
- SDA=17, SCL=18, RST=38, INT=NC (−1)
- I2C addr 0x5D (default when INT floats); alt 0x14
- Driven manually over `Wire1` (see 2026-05-19 entry) — NOT via LovyanGFX

### I2C Bus (shared: touch + sensors on Wire1)
- SDA=17, SCL=18, 100 kHz

### SD Card (SPI)
- CS=10, MOSI=11, SCK=12, MISO=13

### I2S Audio (onboard speaker amp, V2.0+/V3.1)
- BCLK=20, **LRCLK=2**, **DOUT=19** (= I2S_DIN on the amp)
- Per Makerfabs README — easy to swap LRCLK/DOUT by accident; if audio is
  white noise that dips on beats, the two are swapped.
- ⚠️ GPIO 19/20 = USB D±; disable USB CDC if I2S is used here

### Sensors (Mabee I2C → Wire1)
- SGP30 TVOC/eCO2: addr 0x58 (fixed)
- SHT31 Temp/Humidity: addr 0x44 (alt 0x45)
- Mabee GPIO light sensor: `LIGHT_SENSOR_PIN = -1` (disabled — no free
  ADC-capable GPIO; ADC1 = GPIO 1–10, ADC2 = GPIO 11–20 all occupied)

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
