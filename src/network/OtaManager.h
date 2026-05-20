#pragma once
#include <Arduino.h>
#include <functional>

// Thin wrapper around ArduinoOTA. begin() must be called only after WiFi
// is connected. loop() must be called every cycle to service OTA requests.
class OtaManager {
public:
    using StartCb    = std::function<void()>;
    using ProgressCb = std::function<void(uint8_t percent)>;
    using EndCb      = std::function<void()>;
    using ErrorCb    = std::function<void(const char* msg)>;

    void begin(const char* hostname);
    void loop();

    bool isActive()   const { return _active; }
    bool isUpdating() const { return _updating; }

    // Optional UI hooks. Set before begin().
    void onStart   (StartCb    cb) { _onStart    = std::move(cb); }
    void onProgress(ProgressCb cb) { _onProgress = std::move(cb); }
    void onEnd     (EndCb      cb) { _onEnd      = std::move(cb); }
    void onError   (ErrorCb    cb) { _onError    = std::move(cb); }

private:
    bool _active   = false;
    bool _updating = false;

    StartCb    _onStart;
    ProgressCb _onProgress;
    EndCb      _onEnd;
    ErrorCb    _onError;
};
