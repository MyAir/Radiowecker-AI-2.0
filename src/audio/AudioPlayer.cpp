#include "AudioPlayer.h"
#include "../config.h"

// ---------------------------------------------------------------------------
// Stream buffer size in PSRAM (16 KB)
// ---------------------------------------------------------------------------
static constexpr size_t STREAM_BUFFER_SIZE = 16 * 1024;

void AudioPlayer::begin() {
    _output = new AudioOutputI2S();
    _output->SetPinout(I2S_BCLK_PIN, I2S_LRCLK_PIN, I2S_DOUT_PIN);
    _output->SetGain(static_cast<float>(_volume) / 21.0f);
    Serial.printf("[Audio] I2S BCLK=%d LRCLK=%d DOUT=%d, vol=%d\n",
                  I2S_BCLK_PIN, I2S_LRCLK_PIN, I2S_DOUT_PIN, _volume);
}

void AudioPlayer::loop() {
    if (_generator && _generator->isRunning()) {
        if (!_generator->loop()) {
            Serial.println("[Audio] playback ended");
            stop();
        }
    }
}

void AudioPlayer::playStream(const char* url) {
    _cleanup();
    Serial.printf("[Audio] stream: %s\n", url);
    _icy    = new AudioFileSourceICYStream(url);
    _buffer = new AudioFileSourceBuffer(_icy, STREAM_BUFFER_SIZE);
    _generator = new AudioGeneratorMP3();
    _generator->begin(_buffer, _output);
}

void AudioPlayer::playFile(const char* path) {
    _cleanup();
    Serial.printf("[Audio] file: %s\n", path);
    _file      = new AudioFileSourceSD(path);
    _generator = new AudioGeneratorMP3();
    _generator->begin(_file, _output);
}

void AudioPlayer::stop() {
    _cleanup();
}

void AudioPlayer::setVolume(uint8_t vol) {
    _volume = min(vol, static_cast<uint8_t>(21));
    if (_output) {
        _output->SetGain(static_cast<float>(_volume) / 21.0f);
    }
}

bool AudioPlayer::isPlaying() const {
    return _generator && _generator->isRunning();
}

void AudioPlayer::_cleanup() {
    if (_generator) { _generator->stop(); delete _generator; _generator = nullptr; }
    if (_buffer)    { _buffer->close();   delete _buffer;    _buffer    = nullptr; }
    if (_icy)       { _icy->close();      delete _icy;       _icy       = nullptr; }
    if (_http)      { _http->close();     delete _http;      _http      = nullptr; }
    if (_file)      { _file->close();     delete _file;      _file      = nullptr; }
}
