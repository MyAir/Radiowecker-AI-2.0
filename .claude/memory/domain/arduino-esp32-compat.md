# Domain: Arduino-ESP32 3.x Compatibility

## 2026-05-15 — Sensor Library Issues

| Library | Issue | Fix |
|---------|-------|-----|
| Adafruit SHT31 v2.2.2 | `begin()` takes only `uint8_t addr` — no Wire port arg | Pass `Wire1` via constructor: `Adafruit_SHT31 _sht31{&Wire1};` |
| Adafruit SGP30 v2.0+ | `getAbsoluteHumidity()` static method removed | Provide own helper; use `setHumidity(uint32_t)` |

## 2026-05-15 — Arduino-ESP32 3.x Breaking Changes

- **SPIFFS removed**: `#include "SPIFFS.h"` fails. Use LittleFS instead. For third-party libs that include SPIFFS.h: add `include/SPIFFS.h` shim containing `#pragma once\n#include <FS.h>`.
- **`NetworkManager` class conflict**: Framework 3.x ships its own `NetworkManager` in the WiFi stack. Rename any user-defined class (used `WiFiConnector` in this project).
- **`NetworkClient` doesn't exist in 3.0.0**: `NetworkClient` was anticipated for a future 3.x release but is NOT in `framework-arduinoespressif32 @ 3.0.0+sha.ec01775`. `HTTPClient::getStreamPtr()` returns `WiFiClient*` in this version.
- **`LittleFS.begin(true)`**: The `true` arg formats on first use (like `SPIFFS.begin(true)`). Call in `setup()` before any filesystem access.

## 2026-05-15 — LVGL 9.2.2 on ESP32-S3 (Xtensa)

LVGL 9.x ships ARM Helium + NEON `.S` files that fail Xtensa assembler. Pre-build patch script stubs them out — see `tools/platformio.md`.

## 2026-05-16 — ESP8266Audio 1.9.9 + Arduino-ESP32 3.0.0

Two incompatibilities require `scripts/patch_esp8266audio.py` (pre-build extra_script):

1. **SPIFFS usage**: `AudioFileSourceFS.cpp` and `AudioOutputSPIFFSWAV.cpp` include SPIFFS. Patch stubs both files to empty (they are unused in this project).

2. **NetworkClient**: Three files use `NetworkClient` type which doesn't exist. Replace with `WiFiClient`:
   - `AudioFileSourceHTTPStream.h`: `NetworkClient client;` → `WiFiClient client;`
   - `AudioFileSourceHTTPStream.cpp`: `NetworkClient *stream = http.getStreamPtr();` → `WiFiClient *stream = ...`
   - `AudioFileSourceICYStream.cpp`: same replacement

Patches are idempotent (skip if already applied). Both the SPIFFS shim (`include/SPIFFS.h`) and this patch script are needed together.
