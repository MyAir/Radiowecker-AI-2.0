# Domain: Audio (ESP8266Audio on MaTouch V3.1)

Live since 2026-05-22. SD-MP3 playback and HTTP/ICY MP3 streaming (SRF 3
verified) both work end-to-end with the onboard speaker amp.

## 2026-05-22 — Pin mapping for onboard speaker amp

Per Makerfabs README (`Version Attention` → `V2.0` table, same on V3.1):

- `I2S_BCLK_PIN  = 20`
- `I2S_LRCLK_PIN = 2`   (= I2S_LRCLK on the amp)
- `I2S_DOUT_PIN  = 19`  (= I2S_DIN on the amp)

⚠️ GPIO 19/20 are USB D±. USB-CDC and USB-Serial-JTAG must be disabled;
serial goes through UART0/CP2104.

⚠️ Swapping LRCLK and DOUT produces a textbook "white noise that dips on each
beat" signature: amp interprets the data-line as word-clock (random LR
switches) and the LRCLK as data (steady ~44 kHz square wave → broadband
noise). Decode and timing all look fine in the log.

## 2026-05-22 — Architecture

- Dedicated FreeRTOS task `audio` on Core 1, prio 2, 24 KB stack, 8-slot
  command queue. `esp_task_wdt_add(nullptr)` + `esp_task_wdt_reset()` per
  pass; `vTaskDelay(1)` between `mp3->loop()` calls.
- `AudioOutputI2S` default: 44.1 kHz, 16-bit stereo, `STAND_I2S`, APLL on.
  Called with the 3-arg `SetPinout(bclk, wclk, dout)` — `mclkPin` keeps its
  default of `0` but ESP-IDF doesn't drive MCLK unless the amp uses it.
- Volume slider 0..21 → software gain. Default 10 ≈ 0.48 → audible but quiet;
  user typically wants 15–21.

## 2026-05-22 — ID3v2 skip in AudioPlayer

libmad's input buffer is small; if a tagged MP3 begins with a large ID3v2
tag, decoder enters a `MAD_ERROR_BUFLEN` (err=257) loop. Fix: in
`CMD_PLAY_FILE`, peek the first 10 bytes; if they start with `ID3`, parse
the synchsafe size and seek past `10 + size` (`+10` more if footer bit 4 in
the flags is set) before calling `mp3->begin(src, out)`.

## 2026-05-22 — PlatformIO lib patch gotcha

`.pio/libdeps/{env}/` is per-env. If you ever patch a library directly
(debug instrumentation, etc.), mirror the change to BOTH `matouch43` and
`matouch43_ota` trees and touch the file's mtime to force SCons to rebuild:

```powershell
(Get-Item .pio\libdeps\matouch43_ota\ESP8266Audio\src\AudioGeneratorMP3.cpp).LastWriteTime = Get-Date
```

Build log capture in PowerShell: `*>` is UTF-16 — use
`2>&1 | Tee-Object -FilePath build.txt` instead. If `Get-Content` is
shadowed, use `Microsoft.PowerShell.Management\Get-Content`.
