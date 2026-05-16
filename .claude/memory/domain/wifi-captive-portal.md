# Domain: WiFi Captive Portal

## 2026-05-16 — Implementation Pattern (ESP32, Arduino-ESP32 3.x)

**Class**: `WiFiConnector` (in `src/network/NetworkManager.h/.cpp`)  
**Credentials file**: `/wifi.json` on SD card (`{"ssid":"...","password":"..."}`)  
**Config define**: `SD_WIFI_FILE "/wifi.json"` in `src/config.h`

### Flow

```
connect() called
  → load /wifi.json from SD
  → WiFi.begin(ssid, pass) with 20 s timeout
  → on failure or no credentials → _startPortal()

_startPortal()
  → WiFi.scanNetworks() in STA mode (collect SSID/RSSI)
  → WiFi.softAP("Radiowecker-Setup")   ← open, no password
  → DNSServer port 53, wildcard "*" → 192.168.4.1
  → WebServer port 80

loop()
  → _dns->processNextRequest()
  → _server->handleClient()
  → check _restartAt (ESP.restart() after 2 s delay)
```

### Portal Routes

| Route | Method | Purpose |
|-------|--------|---------|
| `/` | GET | HTML page: network dropdown + password field |
| `/save` | POST | Write wifi.json to SD, schedule restart in 2 s |
| `/generate_204` | GET | Android captive portal detection → redirect |
| `/hotspot-detect.html` | GET | iOS/macOS captive portal detection → redirect |
| `/ncsi.txt` | GET | Windows captive portal detection → redirect |
| `/connecttest.txt` | GET | Windows captive portal detection → redirect |

All captive portal detection routes redirect to `http://192.168.4.1/`.

### Key Details

- AP IP: `192.168.4.1` (ESP32 softAP default)
- AP SSID defined as `#define AP_SSID "Radiowecker-Setup"` (must be `#define`, not `constexpr char[]`, to allow string literal concatenation in `Serial.println("..." AP_SSID "...")`)
- Restart is delayed 2 s after `/save` to let the HTTP response reach the browser before the AP disappears
- WiFiScan must run in STA mode **before** switching to AP mode
- `main.cpp`: call `LittleFS.begin(true)` before `network.connect()`; call `network.loop()` in `loop()`
