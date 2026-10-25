#pragma once

// Hardware pin assignments live in include/HardwareConfig.h
#include "HardwareConfig.h"

// ---------------------------------------------------------------------------
// NTP / Time
// ---------------------------------------------------------------------------
#define NTP_SERVER      "pool.ntp.org"
#define NTP_TZ          "CET-1CEST,M3.5.0,M10.5.0/3"   // Central Europe, automatic DST

// ---------------------------------------------------------------------------
// LittleFS paths
// ---------------------------------------------------------------------------
#define CONFIG_FILE     "/config.json"

// ---------------------------------------------------------------------------
// SD card paths
// ---------------------------------------------------------------------------
#define SD_WIFI_FILE    "/wifi.json"   // WiFi credentials on SD card

// ---------------------------------------------------------------------------
// WiFi captive-portal AP
// ---------------------------------------------------------------------------
#define WIFI_AP_SSID    "Radiowecker-Setup"

// ---------------------------------------------------------------------------
// Network hostname (mDNS + OTA target — reachable as <name>.local)
// ---------------------------------------------------------------------------
#define NET_HOSTNAME    "radiowecker2"

// ---------------------------------------------------------------------------
// Defaults (overridden by config.json at runtime)
// ---------------------------------------------------------------------------
#define DEFAULT_VOLUME  10            // 0–21
#define DEFAULT_STREAM  "http://liveradio.swr.de/sw282p3/swr3/play.mp3"

// ---------------------------------------------------------------------------
// Debug log channels (1 = enabled, 0 = silenced)
// ---------------------------------------------------------------------------
#define LOG_TOUCH       1   // GT911 multi-touch coordinates / release
#define LOG_SENSORS     0   // SGP30/SHT31 init + periodic readings
#define LOG_DISPLAY     0   // LGFX/LVGL init + flush-rate diagnostic
