# Domain: Arduino-ESP32 Compatibility

> **2026-05-20 status**: project is on `arduino-esp32 2.0.17` (ESP-IDF 4.4.7) —
> reverted from 3.x to fix the LCD_CAM display glitch (see
> `esp32-s3-lvgl.md`). On 2.0.17, `SPIFFS.h` exists, `NetworkClient`/3.x
> `NetworkManager` collisions don't apply, and `i2c-ng` is not used. The notes
> below document 3.x breakage and apply ONLY if upgrading. Audio is currently
> stubbed (no ESP8266Audio).

## 2026-05-16 — LittleFS Partition Label Bug

```cpp
// WRONG — looks for partition labeled "spiffs" (default):
LittleFS.begin(true)

// CORRECT — partition in partitions.csv is named "littlefs":
LittleFS.begin(true, "/littlefs", 10, "littlefs")
```
`partitions.csv` has: `littlefs, data, spiffs, 0xC10000, 0x3E0000` —
name="littlefs", subtype=spiffs. `LittleFS.begin()` searches by partition
**name**, not subtype.

## 2026-05-16 — LittleFS First-Boot [E] Message is Expected

`LittleFS.begin(true)` (formatOnFail) logs `[E][LittleFS.cpp:98] begin(): Mounting LittleFS failed!` internally before it formats the blank partition. This is **not a real error** — if `begin(true)` returns `true`, the format+remount succeeded. The `[E]` only appears on first boot after a full flash erase; subsequent boots mount silently.

## 2026-05-16 — NTP: Use configTime() not NTPClient

**Problem**: `NTPClient` + `WiFiUDP::beginPacket()` logs `[E][WiFiUdp.cpp:239] beginPacket(): could not get host from dns: 11` when DNS returns EAGAIN (transient failure). Occurs even when WiFi is connected; guarding with `WiFi.status() == WL_CONNECTED` is not sufficient.

**Fix**: Remove `arduino-libraries/NTPClient` from `lib_deps`. Use ESP32 built-in SNTP:

```cpp
// sync() — call after WiFi connects
configTime(NTP_UTC_OFFSET + NTP_DST_OFFSET, 0, NTP_SERVER);
struct tm timeinfo{};
if (getLocalTime(&timeinfo, 10000)) { /* synced */ }
// update() — call from loop(), non-blocking
struct tm t{};
if (!_synced && getLocalTime(&t, 0)) { _synced = true; }
// now()
time_t epoch = time(nullptr);
localtime_r(&epoch, &t);  // TZ set by configTime()
```

`configTime()` handles DNS retries internally; never logs to serial on failure. SNTP re-syncs in the background automatically (no manual `update()` needed after first sync).

## 2026-05-16 — ESP8266Audio 1.9.9 + Arduino-ESP32 3.0.0

Two incompatibilities require `scripts/patch_esp8266audio.py` (pre-build extra_script):

1. **SPIFFS usage**: `AudioFileSourceFS.cpp` and `AudioOutputSPIFFSWAV.cpp` include SPIFFS. Patch stubs both files to empty (they are unused in this project).

2. **NetworkClient**: Three files use `NetworkClient` type which doesn't exist. Replace with `WiFiClient`:
   - `AudioFileSourceHTTPStream.h`: `NetworkClient client;` → `WiFiClient client;`
   - `AudioFileSourceHTTPStream.cpp`: `NetworkClient *stream = http.getStreamPtr();` → `WiFiClient *stream = ...`
   - `AudioFileSourceICYStream.cpp`: same replacement

Patches are idempotent (skip if already applied). Both the SPIFFS shim (`include/SPIFFS.h`) and this patch script are needed together.

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
