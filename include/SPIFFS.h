// Compatibility shim — Arduino-ESP32 3.x removed the SPIFFS library.
// The .cpp files that actually use SPIFFS are stubbed out by
// scripts/patch_esp8266audio.py, so this header only needs to exist to
// satisfy any #include "SPIFFS.h" in library headers.
// <FS.h> is the Arduino base filesystem header, always available.
#pragma once
#include <FS.h>
