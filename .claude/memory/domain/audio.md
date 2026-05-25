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

- Dedicated FreeRTOS task `audio` on **Core 0** (was Core 1 initially), prio 2,
  24 KB stack, 8-slot command queue. `vTaskDelay(1)` between `mp3->loop()`
  calls. Audio task is NOT registered with the task WDT.
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

## 2026-05-23 — Core 0 + IDLE0 WDT removal (final architecture)

Audio task moved from Core 1 → **Core 0** so blocking SD-SPI reads and libmad
decode work never starve LVGL/touch on Core 1. The screen-freeze symptom
during file playback disappeared immediately.

Once on Core 0 a different WDT fired: `Task watchdog got triggered. CPU 0:
IDLE0`. Cause: `AudioGeneratorMP3::loop()` has an internal `goto retry` loop
that calls `Input()` (blocking SD read, SPI polling) on every recoverable
decode error. `ErrorToFlow()` only calls `yield()` = `taskYIELD()`, which
does NOT yield to IDLE (prio 0 < audio prio 2). With WiFi also on Core 0,
IDLE0 can be starved for >5 s on a corrupt MP3.

Fix in `AudioPlayer::begin()` after `xTaskCreatePinnedToCore`:
```cpp
#include <esp_task_wdt.h>
esp_task_wdt_delete(xTaskGetIdleTaskHandleForCPU(0));
```
IDLE1 on Core 1 still guards genuine hangs; the interrupt WDT still catches
true hard lockups.

## 2026-05-23 — AudioFileSourceBuffer is NOT async on SD

`AudioFileSource::readNonBlock()` (base class) just calls blocking `read()`.
`AudioFileSourceBuffer::fill()` therefore does synchronous SD reads from
inside `_mp3->loop()`. Worse, its first call does a single
`SD.read(buf, 32768)` which on slow cards exceeds 5 s and trips the WDT.

→ For `CMD_PLAY_FILE`: feed `AudioGeneratorMP3` directly from
`AudioFileSourceSD`. No buffer. libmad reads ~1536 B per `Input()` (~4 ms),
well within the WDT window between each `vTaskDelay(1)`.

→ For `CMD_PLAY_STREAM`: keep `AudioFileSourceBuffer` (32 KB in PSRAM) — it
absorbs WiFi jitter, and `AudioFileSourceICYStream` doesn't have the same
giant-blocking-read pathology.

## 2026-05-23 — Large ID3v2: read-and-discard, not seek()

`AudioFileSourceSD::seek(big_offset)` traverses the FAT cluster chain and
can stall SPI for several seconds → WDT. Replace with a chunked
read-and-discard (256 B at a time, `vTaskDelay(1)` between chunks). On the
test card ~25 KB skip completes in ~160 ms and leaves the UI fully
responsive.

## 2026-05-23 — Verified MP3 compatibility

Working files end-to-end with this pipeline:
- 128 kbps CBR MP3 (with ID3v2)
- 316 kbps CBR MP3
- VBR 170–210 kbps MP3
- ICY HTTP stream (SRF 3, 128 kbps)

Hours-long silent libmad retries before any audio comes out = the MP3 is
broken (e.g. truncated, random data after ID3). Confirmed by Audacity
refusing the same file. No code fix possible/needed.


## 2026-05-24 — Verified audio URLs / filenames

- SD MP3: `/Chef316.mp3` (file must exist at root of SD card)
- SRF 3 webstream: `http://stream.srg-ssr.ch/m/drs3/mp3_128`
  ⚠️ Use HTTP, not HTTPS — ESP8266Audio's HTTP client does not handle HTTPS natively.
  (`https://` → `[Audio] HTTP open failed`)

