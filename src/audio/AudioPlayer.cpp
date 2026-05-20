#include "AudioPlayer.h"
#include "../serial_safe.h"

// -----------------------------------------------------------------------------
// AudioPlayer — stub implementation. See AudioPlayer.h for context.
// -----------------------------------------------------------------------------

void AudioPlayer::begin() {
    serial_safe_println("[Audio] stub — audio playback disabled until ESP8266Audio is replaced by audio-tools");
}

void AudioPlayer::loop() {
    // nothing to do
}

void AudioPlayer::playStream(const char* url) {
    serial_safe_printf("[Audio] (stub) playStream ignored: %s\n", url ? url : "(null)");
    _playing = false;
}

void AudioPlayer::playFile(const char* path) {
    serial_safe_printf("[Audio] (stub) playFile ignored: %s\n", path ? path : "(null)");
    _playing = false;
}

void AudioPlayer::stop() {
    _playing = false;
}

void AudioPlayer::setVolume(uint8_t vol) {
    _volume = vol > 21 ? 21 : vol;
}
