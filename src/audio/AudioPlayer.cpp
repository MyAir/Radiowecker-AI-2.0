#include "AudioPlayer.h"
#include "../config.h"
#include "../serial_safe.h"

#include <SD.h>
#include <esp_task_wdt.h>
#include <AudioGeneratorMP3.h>
#include <AudioFileSourceSD.h>
#include <AudioFileSourceICYStream.h>
#include <AudioFileSourceBuffer.h>
#include <AudioOutputI2S.h>

// -----------------------------------------------------------------------------
// AudioPlayer — ESP8266Audio implementation, decoding on a dedicated task.
//
// I2S pins come from include/HardwareConfig.h. The default pins (GPIO 19/20)
// collide with native USB on the ESP32-S3: when audio is active the USB
// peripheral is taken over and USB-CDC monitor ports go silent until reset.
// Route Serial through UART0 (CP2104 USB-to-UART) when those pins are used.
// -----------------------------------------------------------------------------

// PSRAM stream buffer for both SD and HTTP playback. 32 KB is enough to
// absorb short SD-SPI / WiFi stalls without underrunning the I2S DMA.
static constexpr uint32_t STREAM_BUF_BYTES = 32 * 1024;

// Audio task config. Pinned to Core 1 (same core as Arduino's loopTask /
// LVGL) for two reasons:
//   1. SD.begin()/SPI.begin() ran on Core 1, and the arduino-esp32 SD
//      driver isn't reliably cross-core safe — reading the same File from
//      Core 0 returned garbage bytes which made the MP3 decoder error on
//      every frame.
//   2. Core 0 also runs WiFi / lwIP at high priority; sustained MP3 work
//      there starved IDLE0 and tripped the task watchdog.
// Priority 2 keeps us above loopTask (prio 1) so audio gets CPU promptly,
// and vTaskDelay(1) at the bottom of every pass guarantees IDLE/LVGL run.
// libmad's mad_frame_decode alone uses ~3-4 KB; 8 KB blew the stack
// silently on first decode (hang, no further printfs). 24 KB gives margin.
static constexpr uint32_t AUDIO_TASK_STACK    = 24 * 1024;
static constexpr UBaseType_t AUDIO_TASK_PRIO  = 2;
static constexpr BaseType_t  AUDIO_TASK_CORE  = 1;

// ---------------------------------------------------------------------------
// Public API — all non-blocking, just enqueues commands for the audio task.
// ---------------------------------------------------------------------------
void AudioPlayer::begin() {
    _queue = xQueueCreate(8, sizeof(Cmd));
    xTaskCreatePinnedToCore(_taskTrampoline, "audio",
                            AUDIO_TASK_STACK, this,
                            AUDIO_TASK_PRIO, &_task, AUDIO_TASK_CORE);
    serial_safe_printf("[Audio] task started on core %d (I2S BCLK=%d LRCLK=%d DOUT=%d)\n",
                       AUDIO_TASK_CORE, I2S_BCLK_PIN, I2S_LRCLK_PIN, I2S_DOUT_PIN);
}

void AudioPlayer::playFile(const char* path) {
    if (!_queue || !path) return;
    Cmd c{};
    c.type = CMD_PLAY_FILE;
    strncpy(c.path, path, sizeof(c.path) - 1);
    xQueueSend(_queue, &c, 0);
}

void AudioPlayer::playStream(const char* url) {
    if (!_queue || !url) return;
    Cmd c{};
    c.type = CMD_PLAY_STREAM;
    strncpy(c.path, url, sizeof(c.path) - 1);
    xQueueSend(_queue, &c, 0);
}

void AudioPlayer::stop() {
    if (!_queue) return;
    Cmd c{};
    c.type = CMD_STOP;
    xQueueSend(_queue, &c, 0);
}

void AudioPlayer::setVolume(uint8_t vol) {
    if (vol > 21) vol = 21;
    _volume = vol;                         // immediate, so volume() returns it
    if (!_queue) return;
    Cmd c{};
    c.type   = CMD_VOLUME;
    c.volume = vol;
    xQueueSend(_queue, &c, 0);
}

// ---------------------------------------------------------------------------
// Audio task — owns the ESP8266Audio objects, drives the decoder loop.
// ---------------------------------------------------------------------------
void AudioPlayer::_taskTrampoline(void* arg) {
    static_cast<AudioPlayer*>(arg)->_taskLoop();
}

void AudioPlayer::_taskLoop() {
    _out = new AudioOutputI2S();
    _out->SetPinout(I2S_BCLK_PIN, I2S_LRCLK_PIN, I2S_DOUT_PIN);
    _out->SetOutputModeMono(false);
    _applyGain();

    // Register this task with the task watchdog and feed it ourselves;
    // mp3->loop() can occasionally take longer than a single tick on the
    // first frames after a sync-search through a large ID3v2 tag.
    esp_task_wdt_add(nullptr);

    Cmd c;
    for (;;) {
        esp_task_wdt_reset();
        // Pull commands. When idle, block up to 20 ms so we don't burn CPU;
        // when playing, only poll the queue and spend the rest decoding.
        const TickType_t wait = _mp3 ? 0 : pdMS_TO_TICKS(20);
        while (xQueueReceive(_queue, &c, wait) == pdTRUE) {
            _handleCmd(c);
        }

        if (_mp3) {
            if (_mp3->isRunning()) {
                if (!_mp3->loop()) {
                    _mp3->stop();
                    serial_safe_println("[Audio] playback ended");
                    _stopInternal();
                }
            } else {
                _stopInternal();
            }
            vTaskDelay(1);
        }
    }
}

void AudioPlayer::_handleCmd(const Cmd& c) {
    switch (c.type) {
        case CMD_VOLUME:
            _volume = c.volume;
            _applyGain();
            break;

        case CMD_STOP:
            if (_playing) serial_safe_println("[Audio] stop");
            _stopInternal();
            break;

        case CMD_PLAY_FILE: {
            _stopInternal();
            serial_safe_printf("[Audio] playFile: %s\n", c.path);
            auto* sd = new AudioFileSourceSD(c.path);
            if (!sd->isOpen()) {
                serial_safe_printf("[Audio] file not found on SD: %s\n", c.path);
                delete sd;
                return;
            }

            // If the file has an ID3v2 tag, skip past it: libmad's 1.5 KB
            // input buffer can't accumulate enough data to scan past a
            // large tag and otherwise gives up with MAD_ERROR_BUFLEN.
            {
                uint8_t hdr[10] = {0};
                uint32_t n = sd->read(hdr, sizeof(hdr));
                uint32_t startPos = 0;
                if (n >= 10 && hdr[0] == 'I' && hdr[1] == 'D' && hdr[2] == '3') {
                    uint32_t sz = ((uint32_t)(hdr[6] & 0x7F) << 21) |
                                  ((uint32_t)(hdr[7] & 0x7F) << 14) |
                                  ((uint32_t)(hdr[8] & 0x7F) <<  7) |
                                  ((uint32_t)(hdr[9] & 0x7F));
                    startPos = 10 + sz;
                    if (hdr[5] & 0x10) startPos += 10; // footer
                    serial_safe_printf("[Audio] ID3v2 detected, skipping %u bytes\n",
                                       (unsigned)startPos);
                }
                sd->seek(startPos, SEEK_SET);
            }

            _src = sd;

            // For SD playback we deliberately do NOT wrap in
            // AudioFileSourceBuffer: its first read pulls the full 32 KB
            // synchronously, which on the audio task appeared to hang.
            // The SD library's own sector cache is enough for MP3.
            auto* mp3 = new AudioGeneratorMP3();
            if (!mp3->begin(_src, _out)) {
                serial_safe_println("[Audio] MP3 begin failed (file)");
                delete mp3;
                _stopInternal();
                return;
            }
            _mp3     = mp3;
            _playing = true;
            break;
        }

        case CMD_PLAY_STREAM: {
            _stopInternal();
            serial_safe_printf("[Audio] playStream: %s\n", c.path);

            _bufMem = ps_malloc(STREAM_BUF_BYTES);
            if (!_bufMem) _bufMem = malloc(STREAM_BUF_BYTES);
            if (!_bufMem) {
                serial_safe_println("[Audio] stream buffer alloc failed");
                return;
            }

            auto* icy = new AudioFileSourceICYStream();
            if (!icy->open(c.path)) {
                serial_safe_println("[Audio] HTTP open failed");
                delete icy;
                free(_bufMem); _bufMem = nullptr;
                return;
            }
            _src = icy;
            _buf = new AudioFileSourceBuffer(_src, _bufMem, STREAM_BUF_BYTES);

            auto* mp3 = new AudioGeneratorMP3();
            if (!mp3->begin(_buf, _out)) {
                serial_safe_println("[Audio] MP3 begin failed (stream)");
                delete mp3;
                _stopInternal();
                return;
            }
            _mp3     = mp3;
            _playing = true;
            break;
        }
    }
}

// ---------------------------------------------------------------------------
// Helpers (audio task context only).
// ---------------------------------------------------------------------------
void AudioPlayer::_applyGain() {
    if (!_out) return;
    float g = _volume / 21.0f;
    if (g < 0.0f) g = 0.0f;
    if (g > 1.0f) g = 1.0f;
    _out->SetGain(g);
}

void AudioPlayer::_stopInternal() {
    if (_mp3) {
        if (_mp3->isRunning()) _mp3->stop();
        delete _mp3;
        _mp3 = nullptr;
    }
    delete _buf; _buf = nullptr;
    delete _src; _src = nullptr;
    if (_bufMem) { free(_bufMem); _bufMem = nullptr; }
    _playing = false;
}

