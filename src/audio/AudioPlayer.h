#pragma once
#include <Arduino.h>
#include <AudioFileSourceHTTPStream.h>
#include <AudioFileSourceSD.h>
#include <AudioFileSourceICYStream.h>
#include <AudioGeneratorMP3.h>
#include <AudioOutputI2S.h>
#include <AudioFileSourceBuffer.h>

class AudioPlayer {
public:
    void begin();
    void loop();

    /** Stream internet radio from an HTTP(S) URL. */
    void playStream(const char* url);

    /** Play an MP3 file from the SD card (path e.g. "/alarm.mp3"). */
    void playFile(const char* path);

    void stop();

    /** Volume: 0 (silent) to 21 (max). */
    void setVolume(uint8_t vol);
    uint8_t volume() const { return _volume; }

    bool isPlaying() const;

private:
    AudioOutputI2S*          _output     = nullptr;
    AudioGeneratorMP3*       _generator  = nullptr;
    AudioFileSourceHTTPStream* _http     = nullptr;
    AudioFileSourceICYStream* _icy       = nullptr;
    AudioFileSourceSD*       _file       = nullptr;
    AudioFileSourceBuffer*   _buffer     = nullptr;

    uint8_t _volume = 10;

    void _cleanup();
};
