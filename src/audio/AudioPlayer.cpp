#include "AudioPlayer.h"
#include "../config.h"
#include "../serial_safe.h"
#include "../AppConfig.h"

#include <SD.h>
#include <WiFi.h>
#include <AudioGeneratorMP3.h>
#include <AudioFileSourceSD.h>
#include <AudioFileSourceICYStream.h>
#include <AudioFileSourceBuffer.h>
#include <AudioOutputI2S.h>
#include <esp_task_wdt.h>

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

// Audio task config. Pinned to Core 0 so blocking SD-SPI reads and libmad
// decode work never starve LVGL/touch on Core 1. WiFi tasks (prio 20-23)
// on Core 0 still preempt us as needed. Priority 2 keeps us above the
// Arduino loopTask (prio 1). libmad's mad_frame_decode alone needs
// ~3-4 KB; 8 KB blew the stack silently. 24 KB gives margin.
static constexpr uint32_t AUDIO_TASK_STACK    = 24 * 1024;
static constexpr UBaseType_t AUDIO_TASK_PRIO  = 2;
static constexpr BaseType_t  AUDIO_TASK_CORE  = 0;

// ---------------------------------------------------------------------------
// Public API — all non-blocking, just enqueues commands for the audio task.
// ---------------------------------------------------------------------------
void AudioPlayer::begin() {
    _queue = xQueueCreate(8, sizeof(Cmd));
    xTaskCreatePinnedToCore(_taskTrampoline, "audio",
                            AUDIO_TASK_STACK, this,
                            AUDIO_TASK_PRIO, &_task, AUDIO_TASK_CORE);

    // The audio task legitimately occupies Core 0 for extended periods
    // (MP3 decode, blocking SD-SPI reads, libmad error-recovery loops).
    // Remove IDLE0 from task-WDT supervision so these don't trigger a
    // false panic.  IDLE1 on Core 1 still guards against genuine hangs.
    esp_task_wdt_delete(xTaskGetIdleTaskHandleForCPU(0));

    serial_safe_printf("[Audio] task started on core %d (I2S BCLK=%d LRCLK=%d DOUT=%d)\n",
                       AUDIO_TASK_CORE, I2S_BCLK_PIN, I2S_LRCLK_PIN, I2S_DOUT_PIN);
}

void AudioPlayer::playFile(const char* path, bool loop) {
    if (!_queue || !path) return;
    Cmd c{};
    c.type = CMD_PLAY_FILE;
    c.loop = loop;
    strncpy(c.path, path, sizeof(c.path) - 1);
    xQueueSend(_queue, &c, 0);
}

void AudioPlayer::playStream(const char* url, bool alarmFallback) {
    if (!_queue || !url) return;
    Cmd c{};
    c.type = CMD_PLAY_STREAM;
    c.alarmFallback = alarmFallback;
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
// Metadata cache (audio-task writer / any-task reader).
// ---------------------------------------------------------------------------
void AudioPlayer::metadata(char* out, size_t outLen) const {
    if (!out || outLen == 0) return;
    portENTER_CRITICAL(&_metaMux);
    const char* a = _metaArtist;
    const char* t = _metaTitle;
    bool hasA = a[0] != '\0';
    bool hasT = t[0] != '\0';
    if (hasA && hasT) {
        snprintf(out, outLen, "%s - %s", a, t);
    } else if (hasT) {
        snprintf(out, outLen, "%s", t);
    } else if (hasA) {
        snprintf(out, outLen, "%s", a);
    } else {
        out[0] = '\0';
    }
    portEXIT_CRITICAL(&_metaMux);
}

void AudioPlayer::_clearMetadata() {
    portENTER_CRITICAL(&_metaMux);
    _metaTitle[0]  = '\0';
    _metaArtist[0] = '\0';
    portEXIT_CRITICAL(&_metaMux);
    _metaVersion++;
}

void AudioPlayer::_setMetaField(const char* key, const char* value) {
    if (!key || !value) return;
    // ICY: key == "StreamTitle" (full "Artist - Track" string).
    // ID3: keys include "Title", "Artist", "TIT2", "TPE1", "Album", ...
    bool changed = false;
    portENTER_CRITICAL(&_metaMux);
    if (strcasecmp(key, "StreamTitle") == 0 ||
        strcasecmp(key, "Title")       == 0 ||
        strcasecmp(key, "TIT2")        == 0) {
        if (strncmp(_metaTitle, value, AUDIO_META_MAX) != 0) {
            strncpy(_metaTitle, value, AUDIO_META_MAX - 1);
            _metaTitle[AUDIO_META_MAX - 1] = '\0';
            changed = true;
        }
    } else if (strcasecmp(key, "Artist") == 0 ||
               strcasecmp(key, "TPE1")   == 0) {
        if (strncmp(_metaArtist, value, AUDIO_META_MAX) != 0) {
            strncpy(_metaArtist, value, AUDIO_META_MAX - 1);
            _metaArtist[AUDIO_META_MAX - 1] = '\0';
            changed = true;
        }
    }
    portEXIT_CRITICAL(&_metaMux);
    if (changed) {
        _metaVersion++;
        serial_safe_printf("[Audio] meta %s='%s'\n", key, value);
    }
}

void AudioPlayer::_audioStatusCb(void* cbData, const char* type, bool /*isUnicode*/, const char* str) {
    auto* self = static_cast<AudioPlayer*>(cbData);
    if (!self || !str || !type) return;
    self->_setMetaField(type, str);
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

    Cmd c;
    for (;;) {
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
                    const bool replay = _loopFile && _loopPath[0] != '\0';
                    char replayPath[sizeof(_loopPath)];
                    if (replay) strncpy(replayPath, _loopPath, sizeof(replayPath));
                    _stopInternal();
                    if (replay) {
                        Cmd rc{};
                        rc.type = CMD_PLAY_FILE;
                        rc.loop = true;
                        strncpy(rc.path, replayPath, sizeof(rc.path) - 1);
                        serial_safe_println("[Audio] loop: restarting file");
                        _handleCmd(rc);
                    }
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
            _loopFile = false;
            _loopPath[0] = '\0';
            _stopInternal();
            break;

        case CMD_PLAY_FILE: {
            _stopInternal();
            _clearMetadata();
            _loopFile = c.loop;
            if (c.loop) {
                strncpy(_loopPath, c.path, sizeof(_loopPath) - 1);
                _loopPath[sizeof(_loopPath) - 1] = '\0';
            } else {
                _loopPath[0] = '\0';
            }
            serial_safe_printf("[Audio] playFile: %s%s\n", c.path, c.loop ? " (loop)" : "");
            auto* sd = new AudioFileSourceSD(c.path);
            if (!sd->isOpen()) {
                serial_safe_printf("[Audio] file not found on SD: %s\n", c.path);
                delete sd;
                return;
            }

            // If the file has an ID3v2 tag, skip past it: libmad's 1.5 KB
            // input buffer can't accumulate enough data to scan past a
            // large tag and otherwise gives up with MAD_ERROR_BUFLEN.
            // Use sequential read-and-discard instead of seek(): FAT32
            // cluster-chain traversal for large offsets can stall the SPI
            // bus for several seconds on slow cards.
            {
                uint8_t hdr[10] = {0};
                uint32_t n = sd->read(hdr, sizeof(hdr));
                if (n >= 10 && hdr[0] == 'I' && hdr[1] == 'D' && hdr[2] == '3') {
                    uint32_t sz = ((uint32_t)(hdr[6] & 0x7F) << 21) |
                                  ((uint32_t)(hdr[7] & 0x7F) << 14) |
                                  ((uint32_t)(hdr[8] & 0x7F) <<  7) |
                                  ((uint32_t)(hdr[9] & 0x7F));
                    uint32_t startPos = 10 + sz;
                    if (hdr[5] & 0x10) startPos += 10; // footer
                    serial_safe_printf("[Audio] ID3v2 detected, skipping %u bytes\n",
                                       (unsigned)startPos);
                    // Discard remaining tag bytes with a vTaskDelay(1) between
                    // each chunk so IDLE0 can run and the system WDT does not
                    // trigger during the ~160 ms skip (audio task on Core 0).
                    const uint32_t t0 = millis();
                    uint32_t remaining = startPos - 10;
                    uint8_t skipBuf[256];
                    while (remaining > 0) {
                        uint32_t toRead = remaining < sizeof(skipBuf) ? remaining : sizeof(skipBuf);
                        sd->read(skipBuf, toRead);
                        remaining -= toRead;
                        vTaskDelay(1);
                    }
                    serial_safe_printf("[Audio] ID3v2 skip done in %u ms\n",
                                       (unsigned)(millis() - t0));
                } else {
                    // No ID3 tag — rewind past the 10 bytes we peeked at
                    sd->seek(0, SEEK_SET);
                }
            }

            _src = sd;

            // ID3 callback on the file source (fires when ID3 frames are
            // observed mid-stream); also wired on the generator below.
            _src->RegisterMetadataCB(_audioStatusCb, this);

            // For SD file playback we use the SD source directly (no
            // AudioFileSourceBuffer).  AudioFileSourceBuffer's initial fill
            // is a single blocking SD.read(32 KB) call; on slow cards this
            // exceeds the 5-second IDLE WDT timeout and reboots the board.
            // AudioFileSourceSD::readNonBlock() falls back to blocking read()
            // anyway, so the buffer would provide no real benefit.
            // libmad reads ~1536 B per Input() call (~10 ms at SD speed),
            // well within the WDT window between each vTaskDelay(1).
            auto* mp3 = new AudioGeneratorMP3();
            if (!mp3->begin(_src, _out)) {
                serial_safe_println("[Audio] MP3 begin failed (file)");
                delete mp3;
                _stopInternal();
                return;
            }
            mp3->RegisterMetadataCB(_audioStatusCb, this);
            _mp3     = mp3;
            _playing = true;
            break;
        }

        case CMD_PLAY_STREAM: {
            _stopInternal();
            _clearMetadata();
            _loopFile = false;
            _loopPath[0] = '\0';
            serial_safe_printf("[Audio] playStream: %s\n", c.path);

            // --- Pre-resolve DNS with retry --------------------------------
            // Scheduled alarms sometimes fire after long idle periods where
            // the lwIP DNS cache has gone stale, producing a one-shot
            // "hostByName(): DNS Failed" before the stream even opens.
            // A short retry loop here gives the resolver another chance
            // without blocking the UI (we're on the audio task).
            {
                const char* p = strstr(c.path, "://");
                if (p) {
                    p += 3;
                    const char* slash = strchr(p, '/');
                    const char* colon = strchr(p, ':');
                    const char* end = slash ? slash : (p + strlen(p));
                    if (colon && colon < end) end = colon;
                    char host[128];
                    size_t len = (size_t)(end - p);
                    if (len >= sizeof(host)) len = sizeof(host) - 1;
                    memcpy(host, p, len);
                    host[len] = '\0';
                    IPAddress ip;
                    for (int i = 0; i < 3; ++i) {
                        if (WiFi.hostByName(host, ip) == 1 &&
                            ip != IPAddress(0, 0, 0, 0)) {
                            if (i > 0) {
                                serial_safe_printf(
                                    "[Audio] DNS resolved %s on retry %d\n",
                                    host, i);
                            }
                            break;
                        }
                        serial_safe_printf("[Audio] DNS retry %d for %s\n",
                                           i + 1, host);
                        vTaskDelay(pdMS_TO_TICKS(400));
                    }
                }
            }

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
                if (c.alarmFallback && g_appConfig.alarmFallbackEnabled()) {
                    // Alarm context: try the user-configured fallback file
                    // first, then fall back to the first MP3 found on the SD
                    // card so the user is still woken up.
                    auto findFirstMp3 = [](const char* dirPath, char* out, size_t outLen) -> bool {
                        File dir = SD.open(dirPath);
                        if (!dir || !dir.isDirectory()) { if (dir) dir.close(); return false; }
                        bool found = false;
                        while (File f = dir.openNextFile()) {
                            if (!f.isDirectory()) {
                                const char* nm = f.name();
                                size_t L = nm ? strlen(nm) : 0;
                                if (L > 4 && nm[L-4] == '.' &&
                                    (nm[L-3]=='m'||nm[L-3]=='M') &&
                                    (nm[L-2]=='p'||nm[L-2]=='P') &&
                                    (nm[L-1]=='3')) {
                                    if (nm[0] == '/') {
                                        snprintf(out, outLen, "%s", nm);
                                    } else {
                                        size_t dl = strlen(dirPath);
                                        bool needSlash = (dl > 0 && dirPath[dl-1] != '/');
                                        snprintf(out, outLen, "%s%s%s",
                                                 dirPath, needSlash ? "/" : "", nm);
                                    }
                                    f.close();
                                    found = true;
                                    break;
                                }
                            }
                            f.close();
                        }
                        dir.close();
                        return found;
                    };
                    char fbPath[200] = {0};
                    const String& cfgPath = g_appConfig.alarmFallbackPath();
                    bool ok = false;
                    if (cfgPath.length() > 0 && SD.exists(cfgPath.c_str())) {
                        strncpy(fbPath, cfgPath.c_str(), sizeof(fbPath) - 1);
                        ok = true;
                    } else {
                        if (cfgPath.length() > 0) {
                            serial_safe_printf("[Audio] configured fallback missing on SD: %s\n",
                                               cfgPath.c_str());
                        }
                        ok = findFirstMp3("/music", fbPath, sizeof(fbPath)) ||
                             findFirstMp3("/",      fbPath, sizeof(fbPath));
                    }
                    if (ok) {
                        serial_safe_printf("[Audio] stream failed, falling back to SD: %s\n", fbPath);
                        Cmd fb{};
                        fb.type = CMD_PLAY_FILE;
                        fb.loop = true;
                        strncpy(fb.path, fbPath, sizeof(fb.path) - 1);
                        _handleCmd(fb);
                    } else {
                        serial_safe_println("[Audio] stream failed and no MP3 on SD for fallback");
                    }
                }
                return;
            }
            _src = icy;
            // ICY StreamTitle metadata fires on the source.
            icy->RegisterMetadataCB(_audioStatusCb, this);
            _buf = new AudioFileSourceBuffer(_src, _bufMem, STREAM_BUF_BYTES);

            auto* mp3 = new AudioGeneratorMP3();
            if (!mp3->begin(_buf, _out)) {
                serial_safe_println("[Audio] MP3 begin failed (stream)");
                delete mp3;
                _stopInternal();
                return;
            }
            mp3->RegisterMetadataCB(_audioStatusCb, this);
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

