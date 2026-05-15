# Radiowecker AI 2.0

Radio alarm clock firmware for the **Makerfabs MaTouch ESP32-S3 Parallel TFT 4.3" V3.1**,
built with PlatformIO + Arduino + LVGL 9 + LovyanGFX.

## Hardware

| Component | Interface | Pins |
|-----------|-----------|------|
| ST7262 4.3" IPS 800×480 | RGB565 parallel | see `include/HardwareConfig.h` |
| GT911 capacitive touch | I2C_NUM_1 (polling) | SDA 17, SCL 18, RST 38 |
| Backlight PWM | GPIO 44, **inverted** | R29 hardware mod |
| SGP30 TVOC/eCO2 | I2C (Wire1) | 0x58 |
| SHT31 Temp/Humidity | I2C (Wire1) | 0x44 |
| Light sensor (Mabee GPIO) | ADC | GPIO 22 |
| SD card | SPI | CS 10, MOSI 11, SCK 12, MISO 13 |
| I2S DAC (e.g. MAX98357A) | I2S | BCLK 20, LRCLK 19, DOUT 2* |

*GPIO 19/20 are USB D±. Disable USB CDC (`-DARDUINO_USB_CDC_ON_BOOT=0`) or reroute I2S
to other free GPIOs if native USB is needed.*

## Software Stack

| Library | Purpose |
|---------|---------|
| lovyan03/LovyanGFX 1.2.7 | Display driver + GT911 touch |
| lvgl/lvgl 9.2.2 | UI framework (`lv_conf.h` in `include/`) |
| earlephilhower/ESP8266Audio | Internet radio streaming + SD MP3 via I2S |
| bblanchon/ArduinoJson 7 | Config persistence on LittleFS |
| arduino-libraries/NTPClient | NTP time sync |
| adafruit/Adafruit SGP30 Sensor | TVOC & eCO2 |
| adafruit/Adafruit SHT31 Library | Temperature & Humidity |

## Project Structure

```
include/
├── HardwareConfig.h    ← all pin/address definitions
├── lgfx_config.h       ← LovyanGFX LGFX class (panel + touch + backlight)
└── lv_conf.h           ← LVGL 9.2.2 configuration
src/
├── main.cpp
├── config.h            ← project-level defaults (NTP, file paths…)
├── alarm/              ← AlarmManager — scheduling & LittleFS persistence
├── audio/              ← AudioPlayer  — I2S streaming & SD playback
├── display/            ← DisplayManager — LGFX + LVGL init, flush & touch
├── network/            ← NetworkManager — WiFi (creds from LittleFS)
├── sensors/            ← SensorManager — SGP30, SHT31, light sensor
└── time/               ← TimeManager  — NTP sync
lib/
└── ui/                 ← EEZ Studio generated files (DO NOT EDIT MANUALLY)
data/
└── config.json         ← WiFi creds, audio settings, alarms (uploaded to LittleFS)
partitions.csv          ← 16 MB: OTA×2 + LittleFS 3.9 MB
```

## Getting Started

1. Copy `data/config.json` and fill in your WiFi credentials.
2. Build & flash:
   ```powershell
   pio run
   try { taskkill /F /IM pio.exe /T > $null 2>&1 } catch { } ; pio run -t upload
   ```
3. Upload filesystem:
   ```
   pio run -t uploadfs
   ```
4. Monitor serial output:
   ```
   pio run -t monitor
   ```
5. Reset via monitor: `Ctrl+T` then `Ctrl+D` (twice if needed).

## EEZ Studio UI Integration

The UI is designed in EEZ Studio and auto-generated into `lib/ui/`. **Never edit files
under `lib/ui/` manually.** To add interactive behaviour:

1. In EEZ Studio, add an event on the target widget and choose *Native User Action*.
2. Name the action (e.g. `volume_up`). EEZ Studio will call `action_volume_up()` in C++.
3. Implement `action_volume_up()` in your own source file.

