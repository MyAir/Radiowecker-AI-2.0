# Domain: ESP32-S3 + LVGL 9 + LovyanGFX

## 2026-05-15 — Board: Makerfabs MaTouch ESP32-S3 Parallel TFT 4.3" V3.1

**MCU**: ESP32-S3-WROOM-1-N16R8 — 16 MB Flash, 8 MB OPI PSRAM  
**Board**: `esp32-s3-devkitc-1` in PlatformIO, `board_build.arduino.memory_type = qio_opi`

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

- `lv_conf.h` in `include/`; build flag `-DLV_CONF_INCLUDE_SIMPLE`
- Tick: `LV_TICK_CUSTOM 1` → `millis()` (no manual `lv_tick_inc()` needed)
- Memory: `LV_MEM_CUSTOM 1` → `malloc/free` (uses PSRAM via `heap_caps_malloc`)
- Draw buffers: allocate in PSRAM with `heap_caps_malloc(…, MALLOC_CAP_SPIRAM)`
- Call `lv_timer_handler_run_in_period(5)` from `loop()` — **not** bare `lv_timer_handler()`
  - Bare call runs faster than 1 ms → `lv_tick_elaps()` returns 0 → LVGL logs warning every cycle
  - `run_in_period(5)` skips execution if < 5 ms elapsed (confirmed in LVGL 9.2.2 `lv_timer.h`)

**LVGL 9 flush callback signature:**
```cpp
static void flush_cb(lv_display_t *display, const lv_area_t *area, uint8_t *px_map);
// call lv_display_flush_ready(display) when done
```

**LVGL 9 touch callback signature:**
```cpp
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

## 2026-05-16 — LCD_CAM Interrupt Fix (ESP-IDF 5.4.4+)
