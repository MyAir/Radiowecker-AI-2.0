#pragma once

// LovyanGFX configuration for:
//   Makerfabs MaTouch ESP32-S3 Parallel TFT 4.3" V3.1
//   Panel:   ST7262 800×480 RGB565 parallel
//   Touch:   GT911 capacitive, I2C, polling mode
//   BL:      GPIO44 PWM, inverted (low = full brightness)

#define LGFX_USE_V1
#include <LovyanGFX.hpp>
#include <lgfx/v1/platforms/esp32s3/Panel_RGB.hpp>
#include <lgfx/v1/platforms/esp32s3/Bus_RGB.hpp>
#include "HardwareConfig.h"

class LGFX : public lgfx::LGFX_Device {

    lgfx::Panel_RGB   _panel_instance;
    lgfx::Bus_RGB     _bus_instance;
    // Touch is polled manually via Wire1 in DisplayManager::pollTouch().
    // Using LovyanGFX Touch_GT911 here installs the legacy ESP-IDF i2c driver
    // on I2C_NUM_1, which conflicts with Wire1 (arduino-esp32 3.x i2c-ng)
    // and triggers periodic ESP_ERR_INVALID_STATE in sensor reads.
    lgfx::Light_PWM   _light_instance;

public:
    LGFX() {
        // -----------------------------------------------------------------
        // RGB parallel bus
        // -----------------------------------------------------------------
        {
            auto cfg = _bus_instance.config();
            cfg.panel = &_panel_instance;

            // Pixel clock & sync
            cfg.pin_pclk    = TFT_PCLK_PIN;
            cfg.pin_vsync   = TFT_VSYNC_PIN;
            cfg.pin_hsync   = TFT_HSYNC_PIN;
            cfg.pin_henable = TFT_DE_PIN;

            // Blue[0..4]
            cfg.pin_d0  = TFT_B0_PIN;
            cfg.pin_d1  = TFT_B1_PIN;
            cfg.pin_d2  = TFT_B2_PIN;
            cfg.pin_d3  = TFT_B3_PIN;
            cfg.pin_d4  = TFT_B4_PIN;

            // Green[0..5]
            cfg.pin_d5  = TFT_G0_PIN;
            cfg.pin_d6  = TFT_G1_PIN;
            cfg.pin_d7  = TFT_G2_PIN;
            cfg.pin_d8  = TFT_G3_PIN;
            cfg.pin_d9  = TFT_G4_PIN;
            cfg.pin_d10 = TFT_G5_PIN;

            // Red[0..4]
            cfg.pin_d11 = TFT_R0_PIN;
            cfg.pin_d12 = TFT_R1_PIN;
            cfg.pin_d13 = TFT_R2_PIN;
            cfg.pin_d14 = TFT_R3_PIN;
            cfg.pin_d15 = TFT_R4_PIN;

            // Timing for ST7262 — values confirmed working in commit 8c930a0
            cfg.freq_write        = 16000000;
            cfg.hsync_polarity    = 0;
            cfg.hsync_front_porch = 8;
            cfg.hsync_pulse_width = 4;
            cfg.hsync_back_porch  = 8;
            cfg.vsync_polarity    = 0;
            cfg.vsync_front_porch = 8;
            cfg.vsync_pulse_width = 4;
            cfg.vsync_back_porch  = 8;
            cfg.pclk_idle_high    = 1;

            _bus_instance.config(cfg);
        }
        _panel_instance.setBus(&_bus_instance);

        // -----------------------------------------------------------------
        // Panel
        // -----------------------------------------------------------------
        {
            auto cfg = _panel_instance.config();
            cfg.memory_width  = TFT_WIDTH;
            cfg.memory_height = TFT_HEIGHT;
            cfg.panel_width   = TFT_WIDTH;
            cfg.panel_height  = TFT_HEIGHT;
            cfg.offset_x      = 0;
            cfg.offset_y      = 0;
            _panel_instance.config(cfg);
        }
        {
            auto cfg = _panel_instance.config_detail();
            cfg.use_psram = 1;
            _panel_instance.config_detail(cfg);
        }

        // -----------------------------------------------------------------
        // GT911 touch driver removed.
        // Touch is polled manually from DisplayManager::pollTouch() over
        // Wire1 so only one I2C driver (arduino-esp32 i2c-ng) is bound to
        // I2C_NUM_1, avoiding ESP_ERR_INVALID_STATE on sensor reads.
        // -----------------------------------------------------------------

        // -----------------------------------------------------------------
        // Backlight PWM — inverted: duty 0 = full bright, 255 = off
        // -----------------------------------------------------------------
        {
            auto cfg = _light_instance.config();
            cfg.pin_bl      = TFT_BACKLIGHT_PIN;
            cfg.invert      = true;                // R29 hardware inversion
            cfg.freq        = 44100;
            cfg.pwm_channel = 7;
            _light_instance.config(cfg);
            _panel_instance.light(&_light_instance);
        }

        setPanel(&_panel_instance);
    }

    /**
     * Return the raw PSRAM framebuffer pointer that the GDMA reads.
     * Used by DisplayManager to set up LVGL DIRECT render mode, eliminating
     * the intermediate writePixels copy and its associated cache-coherency race.
     * Valid only after init() has been called.
     */
    uint8_t* getFrameBuffer() { return _bus_instance.getDMABuffer(0); }

    /**
     * Return the back buffer (the one NOT currently scanned by GDMA).
     * DisplayManager renders LVGL content here; call requestSwap() afterwards
     * to make it visible at the next VSYNC_END.
     */
    uint8_t* getBackBuffer() { return _bus_instance.getBackBuffer(); }

    /**
     * Return the front buffer (the one currently scanned by GDMA).
     * Used by DisplayManager to copy "previous frame dirty" rows from front
     * → back, keeping both PSRAM framebuffers coherent under LVGL PARTIAL
     * render mode.  Unlike getFrameBuffer() this correctly tracks which
     * physical buffer is currently the front after swaps.
     */
    uint8_t* getFrontBuffer() { return _bus_instance.getFrontBuffer(); }

    /**
     * Request that GDMA switches from the front buffer to the back buffer at
     * the next VSYNC_END interrupt.  Call this after Cache_WriteBack_Addr has
     * flushed the back buffer to PSRAM.
     */
    void requestSwap() { _bus_instance.requestSwap(); }
};
