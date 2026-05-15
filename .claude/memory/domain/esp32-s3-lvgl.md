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

Timing: `freq=16MHz, hsync/vsync front/back=8, pulse=4, pclk_active_neg=1`

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
- Mabee GPIO light sensor: ADC GPIO22 (verify on schematic)

---

## 2026-05-15 — LovyanGFX 1.2.7 Config Pattern

```cpp
class LGFX : public lgfx::LGFX_Device {
    lgfx::Panel_RGB   _panel;
    lgfx::Bus_RGB     _bus;
    lgfx::Touch_GT911 _touch;
    lgfx::Light_PWM   _light;
    // configure in constructor, then setPanel(&_panel)
};
```
- Include: `#define LGFX_USE_V1` before `#include <LovyanGFX.hpp>`
- `lib_archive = no` required in `platformio.ini`

---

## 2026-05-15 — LVGL 9.2.2 Integration

- `lv_conf.h` in `include/`; build flag `-DLV_CONF_INCLUDE_SIMPLE`
- Tick: `LV_TICK_CUSTOM 1` → `millis()` (no manual `lv_tick_inc()` needed)
- Memory: `LV_MEM_CUSTOM 1` → `malloc/free` (uses PSRAM via `heap_caps_malloc`)
- Draw buffers: allocate in PSRAM with `heap_caps_malloc(…, MALLOC_CAP_SPIRAM)`
- Call `lv_timer_handler()` from `loop()` every iteration (no `delay()`)

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
