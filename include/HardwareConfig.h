#pragma once

// ===========================================================================
// HardwareConfig.h — Makerfabs MaTouch ESP32-S3 Parallel TFT 4.3" V3.1
// Hardware modification: R29 installed → backlight PWM via GPIO 44
// ===========================================================================

// ---------------------------------------------------------------------------
// I2C Bus  (shared: GT911 touch, SGP30 TVOC/eCO2, SHT31 Temp/Humidity)
// ---------------------------------------------------------------------------
#define I2C_SDA_PIN       17
#define I2C_SCL_PIN       18
#define I2C_FREQUENCY     100000UL      // 100 kHz

// ---------------------------------------------------------------------------
// RGB 565 Parallel Display (ST7262 controller, 800×480)
// ---------------------------------------------------------------------------
#define TFT_WIDTH         800
#define TFT_HEIGHT        480

#define TFT_PCLK_PIN      42
#define TFT_VSYNC_PIN     41
#define TFT_HSYNC_PIN     39
#define TFT_DE_PIN        40
#define TFT_BACKLIGHT_PIN 44            // PWM, INVERTED (low = max brightness)

// Blue channel: B0–B4
#define TFT_B0_PIN        8
#define TFT_B1_PIN        3
#define TFT_B2_PIN        46
#define TFT_B3_PIN        9
#define TFT_B4_PIN        1

// Green channel: G0–G5
#define TFT_G0_PIN        5
#define TFT_G1_PIN        6
#define TFT_G2_PIN        7
#define TFT_G3_PIN        15
#define TFT_G4_PIN        16
#define TFT_G5_PIN        4

// Red channel: R0–R4
#define TFT_R0_PIN        45
#define TFT_R1_PIN        48
#define TFT_R2_PIN        47
#define TFT_R3_PIN        21
#define TFT_R4_PIN        14

// ---------------------------------------------------------------------------
// GT911 Capacitive Touch Controller (I2C, polling mode)
// ---------------------------------------------------------------------------
#define TOUCH_I2C_PORT    1             // I2C_NUM_1
#define TOUCH_RST_PIN     38
#define TOUCH_INT_PIN     -1            // NC — polling mode
#define TOUCH_I2C_ADDR    0x5D         // alt: 0x14

// ---------------------------------------------------------------------------
// SD Card (SPI)
// ---------------------------------------------------------------------------
#define SD_CS_PIN         10
#define SD_MOSI_PIN       11
#define SD_SCK_PIN        12
#define SD_MISO_PIN       13

// ---------------------------------------------------------------------------
// I2S Audio Output (e.g. MAX98357A / PCM5102)
// NOTE: GPIO 19/20 are USB-D+/D-. Use only if USB CDC is disabled.
//       Change to other free GPIOs if native USB is needed.
// ---------------------------------------------------------------------------
#define I2S_BCLK_PIN      20
#define I2S_LRCLK_PIN     19
#define I2S_DOUT_PIN      2

// ---------------------------------------------------------------------------
// Mabee GPIO Connector — Light Sensor (analogue input)
// ---------------------------------------------------------------------------
#define LIGHT_SENSOR_PIN  22            // TODO: verify on schematic

// ---------------------------------------------------------------------------
// I2C Device Addresses
// ---------------------------------------------------------------------------
#define SGP30_I2C_ADDR    0x58         // fixed
#define SHT31_I2C_ADDR    0x44         // default (ADDR pin low); alt: 0x45
