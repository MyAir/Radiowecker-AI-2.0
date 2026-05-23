# My Hardware is
- Makerfabs MaTouch_ESP32-S3 Parallel TFT with Touch 4.3" module V3.1
- Github Link: https://github.com/Makerfabs/ESP32-S3-Parallel-TFT-with-Touch-4.3inch/tree/main
- Hardware Diagrams Link: https://github.com/Makerfabs/ESP32-S3-Parallel-TFT-with-Touch-4.3inch/tree/main/hardware
- Wiki Link: https://wiki.makerfabs.com/MaTouch_S3_Parallel_4.3_TFT_with_Touch.html
- Hardware modifications: Installed R29 resistor to allow PWM backlight control via GPIO44

# Features:
- Controller: ESP32-S3-WROOM-1, PCB Antenna, 16MB Flash, 8MB PSRAM, ESP32-S3-WROOM-1-N16R8；
- Wireless: Wifi& Bluetooth 5.0
- LCD: 4.3 inch High Lightness IPS
- FPS: >30
- Resolution: 800*480
- LCD interface: RGB 565
- Touch Panel: 5 Points Touch, Capacitive
- Touch Panel Driver: GT911
- USB: Dual USB Type-C(one for USB-to-UART and one for native USB)
- USB to UART Chip: CP2104
- Power Supply: USB Type-C 5.0V(4.0V~5.25V)
- Button: Flash button and reset button
- Mabee interface: 1*I2C;1*GPIO
- MicroSD: Yes
- Arduino support: Yes
- Connected to the I2C interface: Mabee_TVOC and eCO2 SGP30 & Temperature and Humidity SHT31 module. 

# My Software is
- Visual Studio Code
- PlatformIO 
- ESP32-S3
- LVGL 9.2.2 
- lovyan03/LovyanGFX@^1.2.7
- ESP8266Audio
- Adafruit SGP30 Sensor
- Adafruit SHT31 Library

# PlatformIO Commands to use
- PlatformIO: `pio run` to build the code
- PlatformIO: `pio run -t monitor` to monitor the output of the ESP32-S3
- PlatformIO: `pio run -t clean` to clean the build directory

# Important Notes
- The code should follow best practices for embedded systems development.
- The code should be easy to read and understand.
- The code should be easy to maintain and update.
- The code should be easy to debug.
- Never edit files in .pio These are managed by platformio

# Hardware Configuration for Radiowecker Project

## I2C Bus
- **SDA Pin**: GPIO 17
- **SCL Pin**: GPIO 18
- **Frequency**: 100kHz
- **Shared by**: GT911 Touch, SGP30 Sensor, SHT31 Sensor
- **Source File**: [include/HardwareConfig.h](cci:7://file:///c:/Projekte/Arduino/Radiowecker_EEZ_AI/include/HardwareConfig.h:0:0-0:0)

## GT911 Touch Controller
- **I2C Port**: `I2C_NUM_1`
- **SDA Pin**: GPIO 17 (shared)
- **SCL Pin**: GPIO 18 (shared)
- **Interrupt Pin**: Not Connected (`GPIO_NUM_NC`). Polling mode is used.
- **Reset Pin**: GPIO 38
- **Source File**: [include/lgfx_config.h](cci:7://file:///c:/Projekte/Arduino/Radiowecker_EEZ_AI/include/lgfx_config.h:0:0-0:0)

## Display: 4.3" Parallel RGB TFT
The full initialization is handled by the [LGFX](cci:2://file:///c:/Projekte/Arduino/Radiowecker_EEZ_AI/include/lgfx_config.h:10:0-114:1) class constructor in [include/lgfx_config.h](cci:7://file:///c:/Projekte/Arduino/Radiowecker_EEZ_AI/include/lgfx_config.h:0:0-0:0).

### Key Panel/Bus Config:
- **Resolution**: 800x480
- **PSRAM**: Enabled
- **PCLK**: GPIO 42
- **VSYNC**: GPIO 41
- **HSYNC**: GPIO 39
- **DE**: GPIO 40
- **Data Pins (RGB565)**:
  - **Blue**: B0-B4 on GPIOs 8, 3, 46, 9, 1
  - **Green**: G0-G5 on GPIOs 5, 6, 7, 15, 16, 4
  - **Red**: R0-R4 on GPIOs 45, 48, 47, 21, 14
- **Backlight**: PWM on GPIO 44 (requires hardware modification). Note: The PWM signal is inverted (low signal = max brightness), which is handled by the LovyanGFX driver configuration.

## I2S Audio (onboard speaker amp)
The Makerfabs MaTouch V2.0+ / V3.1 board has an onboard I2S speaker amplifier
with a JST connector for a small speaker. Pin mapping per the Makerfabs
README (`Version Attention` → `V2.0` table):

- **BCLK**: GPIO 20
- **LRCLK (WS)**: GPIO 2
- **DIN (= MCU DOUT)**: GPIO 19
- ⚠️ GPIO 19 and 20 are also USB D+/D−. USB-CDC and USB-Serial-JTAG must be
  disabled when audio is in use (serial then routes through UART0 / CP2104).
- ⚠️ Easy mistake: swapping LRCLK and DOUT produces audible white noise that
  dips on each beat (data-line and word-clock-line are crossed at the amp).
  Verified working order is the one above.
- Software: `ESP8266Audio` (`AudioGeneratorMP3` + `AudioOutputI2S`), I2S at
  44.1 kHz, 16-bit stereo, `STAND_I2S` format. SD-MP3 and HTTP/ICY MP3
  streaming (e.g. SRF 3) both verified.

## Manual Reset Procedure
- The device resets on the **falling edge of the DTR signal**.
- In the PlatformIO serial monitor, this is triggered by toggling DTR from **active to inactive**.
- **Keystrokes**: Press `Ctrl+T` then `Ctrl+D`. This may need to be done twice: once to set DTR active, and a second time to set it inactive to trigger the reset.