#pragma once
#include <Arduino.h>

// -----------------------------------------------------------------------------
// AudioPlayer — temporarily stubbed out.
//
// The original implementation used earlephilhower/ESP8266Audio, which is being
// removed as part of the migration to the Radiowecker_EEZ_AI package stack
// (espressif32 5.4.0 / arduino-esp32 2.0.17 / LovyanGFX 1.2.7). Audio output
// has not been wired up or tested yet on this project, so the manager keeps
// the same public API but performs no audio I/O. It will be re-implemented on
// top of arduino-audio-tools + arduino-libhelix once display + touch are
// glitch-free.
// -----------------------------------------------------------------------------
class AudioPlayer {
public:
    void begin();
    void loop();

    /** Stream internet radio from an HTTP(S) URL. (No-op stub.) */
    void playStream(const char* url);

    /** Play an MP3 file from the SD card (path e.g. "/alarm.mp3"). (No-op stub.) */
    void playFile(const char* path);

    void stop();

    /** Volume: 0 (silent) to 21 (max). */
    void setVolume(uint8_t vol);
    uint8_t volume() const { return _volume; }

    bool isPlaying() const { return _playing; }

private:
    uint8_t _volume  = 10;
    bool    _playing = false;
};
