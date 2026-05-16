#include "SensorManager.h"
#include "../config.h"
#include <math.h>

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
        Serial.println("[Sensors] SGP30 found");
        // The SGP30 requires 15 s warm-up before baseline readings are stable.
        // initAirQuality must be called once to start the measurement cycle.
        _sgp30.IAQinit();
    } else {
        Serial.println("[Sensors] SGP30 NOT found");
    }

    // SHT31
    _sht31Ok = _sht31.begin(SHT31_I2C_ADDR);
    if (_sht31Ok) {
        Serial.println("[Sensors] SHT31 found");
        _sht31.heater(false);   // heater off by default
    } else {
        Serial.println("[Sensors] SHT31 NOT found");
    }

    // Configure ADC for light sensor (only when a valid pin is assigned)
    if (LIGHT_SENSOR_PIN >= 0) analogReadResolution(12);   // 0–4095

    return _sgp30Ok || _sht31Ok;
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
    }

    r.light = (LIGHT_SENSOR_PIN >= 0) ? static_cast<uint16_t>(analogRead(LIGHT_SENSOR_PIN)) : 0;

    return r;
}
