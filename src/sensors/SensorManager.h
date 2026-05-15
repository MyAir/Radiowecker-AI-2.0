#pragma once
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_SGP30.h>
#include <Adafruit_SHT31.h>

/**
 * SensorManager
 *
 * Manages all I2C sensors on the Mabee connector (shared I2C bus, Wire1):
 *   - Adafruit SGP30  — TVOC & eCO2 (I2C 0x58, fixed)
 *   - Adafruit SHT31  — Temperature & Humidity (I2C 0x44 default)
 *   - Mabee Light Sensor — analogue read on LIGHT_SENSOR_PIN
 */
class SensorManager {
public:
    struct Reading {
        float    temperature  = 0.0f;   // °C
        float    humidity     = 0.0f;   // %RH
        uint16_t tvoc         = 0;      // ppb
        uint16_t eco2         = 0;      // ppm
        uint16_t light        = 0;      // raw ADC counts (0–4095)
        bool     valid        = false;
    };

    /** Initialise Wire1 and both sensors. */
    bool begin();

    /**
     * Read all sensors.
     * Call no more frequently than ~1 s (SGP30 requires 1 s interval).
     */
    Reading read();

    bool sgp30Ready()  const { return _sgp30Ok; }
    bool sht31Ready()  const { return _sht31Ok; }

private:
    Adafruit_SGP30 _sgp30;
    Adafruit_SHT31 _sht31{&Wire1};
    bool _sgp30Ok = false;
    bool _sht31Ok = false;
};
