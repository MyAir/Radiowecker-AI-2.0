#include "SensorManager.h"
#include "../config.h"
#include "../serial_safe.h"
#include <math.h>
#include <time.h>
#include <SD.h>
#include <ArduinoJson.h>

static constexpr const char* IAQ_BASELINE_FILE   = "/iaq_baseline.json";
static constexpr uint32_t    IAQ_SAVE_INTERVAL_MS = 60UL * 60UL * 1000UL;        // 1 h
static constexpr uint32_t    IAQ_LEARN_MS         = 12UL * 60UL * 60UL * 1000UL; // 12 h first-run learning
static constexpr time_t      IAQ_MAX_AGE_S        = 7L * 24L * 60L * 60L;        // 7 days
static constexpr time_t      MIN_VALID_EPOCH      = 1600000000;                  // NTP not yet synced below this

// SGP30 requires absolute humidity in g/m³ encoded as 8.8 fixed-point (mg/m³ * 1000).
// The Adafruit_SGP30 library 2.0+ removed the utility static method, so we define it here.
static uint32_t sgp30AbsHumidity(float tempC, float relHumPct) {
    const float ah = 216.7f * ((relHumPct / 100.0f) * 6.112f *
                     expf((17.62f * tempC) / (243.12f + tempC))) /
                     (273.15f + tempC);
    return static_cast<uint32_t>(1000.0f * ah);
}

bool SensorManager::begin() {
    // Initialise shared I2C bus (Wire1 = I2C_NUM_1)
    Wire1.begin(I2C_SDA_PIN, I2C_SCL_PIN, I2C_FREQUENCY);

    // SGP30
    _sgp30Ok = _sgp30.begin(&Wire1);
    if (_sgp30Ok) {
#if LOG_SENSORS
        serial_safe_println("[Sensors] SGP30 found");
#endif
        // The SGP30 requires 15 s warm-up before baseline readings are stable.
        // initAirQuality must be called once to start the measurement cycle.
        _sgp30.IAQinit();
        _beginMs = millis();
        restoreBaseline();
    } else {
#if LOG_SENSORS
        serial_safe_println("[Sensors] SGP30 NOT found");
#endif
    }

    // SHT31
    _sht31Ok = _sht31.begin(SHT31_I2C_ADDR);
    if (_sht31Ok) {
#if LOG_SENSORS
        serial_safe_println("[Sensors] SHT31 found");
#endif
        _sht31.heater(false);   // heater off by default
    } else {
#if LOG_SENSORS
        serial_safe_println("[Sensors] SHT31 NOT found");
#endif
    }

    // Configure ADC for light sensor (only when a valid pin is assigned)
    if (LIGHT_SENSOR_PIN >= 0) analogReadResolution(12);   // 0–4095

    return _sgp30Ok || _sht31Ok;
}

void SensorManager::restoreBaseline() {
    if (!SD.exists(IAQ_BASELINE_FILE)) return;
    File f = SD.open(IAQ_BASELINE_FILE, FILE_READ);
    if (!f) return;
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, f);
    f.close();
    if (err || doc["eco2"].isNull() || doc["tvoc"].isNull()) return;

    // Age can only be checked once the clock is set; otherwise trust the file.
    const time_t now = time(nullptr);
    const int64_t ts = doc["ts"] | 0;
    if (now >= MIN_VALID_EPOCH && ts > 0 && (now - ts) > IAQ_MAX_AGE_S) {
        serial_safe_println("[Sensors] IAQ baseline too old, ignored");
        return;
    }

    const uint16_t eco2 = doc["eco2"];
    const uint16_t tvoc = doc["tvoc"];
    if (_sgp30.setIAQBaseline(eco2, tvoc)) {
        _baselineValid = true;
        _lastSaveMs    = millis();
#if LOG_SENSORS
        serial_safe_printf("[Sensors] IAQ baseline restored: eCO2=0x%04X TVOC=0x%04X\n", eco2, tvoc);
#endif
    }
}

void SensorManager::saveBaseline() {
    const time_t now = time(nullptr);
    if (now < MIN_VALID_EPOCH) return;   // retry next cycle once NTP is synced

    uint16_t eco2, tvoc;
    if (!_sgp30.getIAQBaseline(&eco2, &tvoc)) return;

    JsonDocument doc;
    doc["eco2"] = eco2;
    doc["tvoc"] = tvoc;
    doc["ts"]   = (int64_t)now;

    File f = SD.open(IAQ_BASELINE_FILE, FILE_WRITE);
    if (!f) return;
    serializeJson(doc, f);
    f.close();
    _lastSaveMs = millis();
#if LOG_SENSORS
    serial_safe_printf("[Sensors] IAQ baseline saved: eCO2=0x%04X TVOC=0x%04X\n", eco2, tvoc);
#endif
}

SensorManager::Reading SensorManager::read() {
    Reading r;

    if (_sht31Ok) {
        r.temperature = _sht31.readTemperature();
        r.humidity    = _sht31.readHumidity();
        if (!isnan(r.temperature) && !isnan(r.humidity)) {
            // Provide abs. humidity to SGP30 for better accuracy
            if (_sgp30Ok) {
                _sgp30.setHumidity(
                    sgp30AbsHumidity(r.temperature, r.humidity));
            }
            r.valid = true;
        }
    }

    if (_sgp30Ok) {
        if (_sgp30.IAQmeasure()) {
            r.tvoc  = _sgp30.TVOC;
            r.eco2  = _sgp30.eCO2;
            r.valid = true;
        }

        const uint32_t now = millis();
        if (!_baselineValid && now - _beginMs >= IAQ_LEARN_MS) {
            _baselineValid = true;
            _lastSaveMs    = now - IAQ_SAVE_INTERVAL_MS;   // save right away
        }
        if (_baselineValid && now - _lastSaveMs >= IAQ_SAVE_INTERVAL_MS) {
            saveBaseline();
        }
    }

    r.light = (LIGHT_SENSOR_PIN >= 0) ? static_cast<uint16_t>(analogRead(LIGHT_SENSOR_PIN)) : 0;

    return r;
}
