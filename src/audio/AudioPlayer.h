#pragma once
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>

// Forward declarations of ESP8266Audio classes — keeps the header free of
// the library include so it can be picked up by sources that don't need
// the full class definitions.
class AudioGenerator;
class AudioFileSource;
class AudioFileSourceBuffer;
class AudioOutputI2S;

// -----------------------------------------------------------------------------
// AudioPlayer
//
// MP3 playback over I2S using earlephilhower/ESP8266Audio.
//
// All decoding happens in a dedicated FreeRTOS task pinned to Core 0 so the
// LVGL / main loop on Core 1 is never blocked by SD reads, TCP reads or
// MP3 decode work.  The public API just enqueues commands; nothing in here
// blocks for more than a queue send.
//
//   - playFile():   MP3 from SD card root (e.g. "/ChefVBR170-210.mp3")
//   - playStream(): MP3/ICY shoutcast HTTP stream
//   - setVolume():  0 (silent) .. 21 (max), mapped to ESP8266Audio gain 0..1
//
// I2S pinout is taken from include/HardwareConfig.h
// (BCLK / LRCLK / DOUT). Connect e.g. a MAX98357A breakout to those pins.
// -----------------------------------------------------------------------------
class AudioPlayer {
public:
    void begin();

    /** No-op — kept for source compatibility. Audio runs on its own task. */
    void loop() {}

    /** Stream internet radio from an HTTP MP3/ICY URL. Non-blocking. */
    void playStream(const char* url);

    /** Play an MP3 file from the SD card (path e.g. "/ChefVBR170-210.mp3"). Non-blocking. */
    void playFile(const char* path);

    /** Stop playback. Non-blocking — the audio task tears down on its next tick. */
    void stop();

    /** Volume: 0 (silent) to 21 (max). Non-blocking. */
    void setVolume(uint8_t vol);
    uint8_t volume() const { return _volume; }

    bool isPlaying() const { return _playing; }

private:
    enum CmdType : uint8_t {
        CMD_PLAY_FILE,
        CMD_PLAY_STREAM,
        CMD_STOP,
        CMD_VOLUME,
    };
    struct Cmd {
        CmdType type;
        uint8_t volume;
        char    path[192];
    };

    static void _taskTrampoline(void* arg);
    void        _taskLoop();
    void        _handleCmd(const Cmd& c);
    void        _stopInternal();
    void        _applyGain();

    AudioGenerator*        _mp3     = nullptr;
    AudioFileSource*       _src     = nullptr;
    AudioFileSourceBuffer* _buf     = nullptr;
    AudioOutputI2S*        _out     = nullptr;
    void*                  _bufMem  = nullptr;

    QueueHandle_t _queue   = nullptr;
    TaskHandle_t  _task    = nullptr;

    volatile uint8_t _volume  = 10;
    volatile bool    _playing = false;
};

