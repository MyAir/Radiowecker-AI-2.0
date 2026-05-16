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
    lgfx::Touch_GT911 _touch_instance;
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
        // GT911 touch (I2C, polling)
        // -----------------------------------------------------------------
        {
            auto cfg = _touch_instance.config();
            cfg.x_min           = 0;
            cfg.x_max           = TFT_WIDTH  - 1;
            cfg.y_min           = 0;
            cfg.y_max           = TFT_HEIGHT - 1;
            cfg.pin_int         = TOUCH_INT_PIN;   // NC
            cfg.pin_rst         = TOUCH_RST_PIN;
            cfg.bus_shared      = false;
            cfg.offset_rotation = 0;
            cfg.i2c_port        = TOUCH_I2C_PORT;  // I2C_NUM_1
            cfg.i2c_addr        = TOUCH_I2C_ADDR;
            cfg.pin_sda         = I2C_SDA_PIN;
            cfg.pin_scl         = I2C_SCL_PIN;
            cfg.freq            = 400000;          // 400 kHz for touch
            _touch_instance.config(cfg);
            _panel_instance.setTouch(&_touch_instance);
        }

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
};
