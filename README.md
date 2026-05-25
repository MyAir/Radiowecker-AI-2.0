# Radiowecker AI 2.0

Radio alarm clock firmware for the **Makerfabs MaTouch ESP32-S3 Parallel TFT 4.3" V3.1**,
built with PlatformIO + Arduino + LVGL 9 + LovyanGFX. UI is hand-coded against LVGL
(no EEZ Studio); all user data (WiFi, stations, alarms, weather cache, assets, MP3s)
lives on the SD card.

> **About the code:** ~99% of this firmware was vibe-coded with Anthropic's
> **Claude Sonnet 4.6** and **Claude Opus 4.7** via GitHub Copilot in VS Code.
> The human author drives the hardware, design decisions, and review; the
> models write almost all of the C++/LVGL/PlatformIO code.

## Features

- **Big touchscreen clock** — 800×480 IPS, custom large fonts (up to 120 px),
  German weekday/date, "Next alarm" preview line, brightness slider with
  optional light-sensor auto-dim.
- **Multiple alarms** — independent enable, per-alarm time, weekday mask,
  sound source (internet radio station or local SD-card MP3), and volume.
  Master on/off toggle on the main screen.
- **Snooze & dismiss** — full-screen alarm UI with snooze button and
  enforced max alarm duration (auto-stop) to avoid runaway ringing.
- **Skip next alarm** — one-tap toggle to skip the upcoming occurrence
  of a recurring alarm without disabling the schedule (e.g. for a
  day off); the alarm automatically re-arms for the following trigger.
- **Internet radio** — streams ICY/MP3 stations over WiFi to an I2S DAC
  (MAX98357A or similar). Stations list editable via `stations.json` on SD.
- **Local MP3 playback** — alarm sounds and test tones loaded directly
  from the SD card.
- **Weather panel** — Open-Meteo client with cached forecast and custom
  weather icons.
- **Environment sensors** — SGP30 (TVOC, eCO2) and SHT31 (temperature,
  humidity) displayed on the main screen.
- **WiFi + OTA** — auto-connect from `wifi.json`, ArduinoOTA on
  `radiowecker2.local`, fully wireless updates after the first USB flash.
- **NTP time sync** with German timezone and DST handling.
- **On-device settings UI** — tabbed settings screen with general
  preferences, alarm setup, and a debug/diagnostics panel. Text input
  via on-screen **German QWERTZ keyboard** with custom umlaut support.
- **SD-based configuration** — all settings (`config.json`, `wifi.json`,
  `stations.json`, `alarms.json`, `weather.json`) hot-loaded from SD;
  no firmware reflash required to change content.
- **Robust multi-task design** — separate audio task, Serial mutex for
  safe multi-core logging, watchdog-friendly LVGL tick.

## Hardware

| Component | Interface | Pins |
|-----------|-----------|------|
| ST7262 4.3" IPS 800×480 | RGB565 parallel | see [include/HardwareConfig.h](include/HardwareConfig.h) |
| GT911 capacitive touch | I2C (Wire1) | SDA 17, SCL 18, RST 38 |
| Backlight PWM | GPIO 44, **inverted** (R29 hardware mod) | |
| SGP30 TVOC/eCO2 | I2C (Wire1) | 0x58 |
| SHT31 Temp/Humidity | I2C (Wire1) | 0x44 |
| SD card | SPI | CS 10, MOSI 11, SCK 12, MISO 13 |
| I2S DAC (e.g. MAX98357A) | I2S | BCLK 20, LRCLK 19, DOUT 2* |

*GPIO 19/20 are USB D±. Native USB CDC is disabled in [platformio.ini](platformio.ini)
(`-DARDUINO_USB_CDC_ON_BOOT=0`, `-DARDUINO_USB_MODE=0`) so the I2S DMA on these pins
isn't blocked. Serial monitor goes over the CP2104 UART0 port.*

Hostname: `radiowecker2` (mDNS: `radiowecker2.local`).
Flash: 16 MB QIO. PSRAM: 8 MB OPI.

## Software Stack

| Library | Version | Purpose |
|---------|---------|---------|
| espressif32 platform | 5.4.0 | arduino-esp32 2.0.17 (pinned — newer versions cause per-second horizontal shift on this panel) |
| lovyan03/LovyanGFX | 1.2.7 | Display driver + GT911 touch |
| lvgl/lvgl | 9.2.2 | UI framework ([include/lv_conf.h](include/lv_conf.h)) |
| earlephilhower/ESP8266Audio | ^1.9.9 | Internet radio streaming + SD MP3 via I2S |
| bblanchon/ArduinoJson | ^7 | Config persistence (SD card) |
| adafruit/Adafruit SGP30 Sensor | ^2 | TVOC & eCO2 |
| adafruit/Adafruit SHT31 Library | ^2.2.2 | Temperature & Humidity |

## Project Structure

```
include/
├── HardwareConfig.h    ← all pin/address definitions
├── lgfx_config.h       ← LovyanGFX LGFX class (panel + touch + backlight)
├── lv_conf.h           ← LVGL 9.2.2 configuration
└── SPIFFS.h            ← shim so ESP8266Audio compiles (SPIFFS unused)
src/
├── main.cpp
├── config.h            ← project-level defaults
├── AppConfig.h/.cpp    ← runtime config loaded from SD-Data/config.json
├── serial_safe.h       ← Serial mutex for multi-task logging
├── alarm/              ← AlarmManager — scheduling & SD persistence
├── audio/              ← AudioPlayer + StationsList — I2S streaming & SD playback
├── display/            ← LVGL screens (Main, Settings, AlarmSetup, Alarm, General, Debug)
│                        LovyanGFX init, German QWERTZ keyboard, custom fonts
├── network/            ← NetworkManager (WiFi) + OtaManager (ArduinoOTA)
├── sensors/            ← SensorManager — SGP30, SHT31, light sensor
├── time/               ← TimeManager — NTP sync
└── weather/            ← WeatherManager — Open-Meteo client + cache
scripts/                ← PlatformIO pre-scripts + build helpers
├── patch_lvgl.py            ← stubs ARM-only assembly in LVGL 9.2.2
├── patch_esp8266audio.py    ← stubs unused SPIFFS sources in ESP8266Audio
├── patch_lgfx.py
└── wait_build.ps1           ← poll pio.exe and tail build_out.txt
SD-Data/                ← contents to copy to the SD card root
├── config.json         ← deviceName, brightness, alarms config, etc.
├── wifi.json           ← WiFi credentials
├── stations.json       ← radio stations
├── alarms.json         ← alarm definitions
├── weather.json        ← weather cache
├── *.mp3               ← local alarm/test sounds
└── assets/             ← custom fonts + weather icons
partitions.csv          ← 16 MB: app0 + app1 (OTA) + LittleFS
platformio.ini          ← matouch43 (USB) + matouch43_ota (WiFi) envs
```

## Getting Started

1. Copy the contents of `SD-Data/` to a FAT32-formatted SD card. Edit
   `wifi.json` with your credentials and `config.json` / `stations.json`
   to taste.
2. Insert the SD card, then build & flash over USB:

   ```powershell
   .\.claude\tools\build.ps1                  # build only
   .\.claude\tools\build.ps1 -Target upload   # USB upload (kills monitor first)
   .\.claude\tools\build.ps1 -Target monitor  # serial monitor
   ```

   The wrapper script handles the full `pio.exe` path, kills the serial
   monitor before flashing, and prints a clear SUCCESS / FAILED summary.

3. After the first USB flash, subsequent updates can go over WiFi:

   ```powershell
   .\.claude\tools\build.ps1 -Target upload -Env matouch43_ota
   ```

   The device must be reachable as `radiowecker2.local` (mDNS). To override:
   `pio run -e matouch43_ota -t upload --upload-port 192.168.x.y`.

4. Reset via monitor: `Ctrl+T` then `Ctrl+D`.

## Pre-build Scripts

Both run automatically via `extra_scripts` in [platformio.ini](platformio.ini):

- `scripts/patch_lvgl.py` — stubs out ARM Helium/NEON assembly files that
  break Xtensa builds. Required for LVGL 9.2.2 on ESP32-S3.
- `scripts/patch_esp8266audio.py` — stubs `AudioFileSourceFS.cpp` and
  `AudioOutputSPIFFSWAV.cpp` (unused; our `include/SPIFFS.h` shim provides
  no SPIFFS object).

## Notes

- All persistent settings are loaded from the SD card; there is no
  `pio run -t uploadfs` step. The LittleFS partition exists only to
  satisfy the OTA partition table layout.
- Memory rules and toolchain conventions for AI coding agents live in
  [claude.md](claude.md) and [AGENTS.md](AGENTS.md); per-topic notes are
  in `.claude/memory/`.
