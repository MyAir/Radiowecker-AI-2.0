#pragma once

// Hardware pin assignments live in include/HardwareConfig.h
#include "HardwareConfig.h"

// ---------------------------------------------------------------------------
// NTP / Time
// ---------------------------------------------------------------------------
#define NTP_SERVER      "pool.ntp.org"
#define NTP_UTC_OFFSET  3600          // seconds east of UTC (CET = +1 h)
#define NTP_DST_OFFSET  3600          // additional offset for DST (CEST = +1 h)

// ---------------------------------------------------------------------------
// LittleFS paths
// ---------------------------------------------------------------------------
#define CONFIG_FILE     "/config.json"

// ---------------------------------------------------------------------------
// SD card paths
// ---------------------------------------------------------------------------
#define SD_WIFI_FILE    "/wifi.json"   // WiFi credentials on SD card

// ---------------------------------------------------------------------------
// Defaults (overridden by config.json at runtime)
// ---------------------------------------------------------------------------
#define DEFAULT_VOLUME  10            // 0–21
#define DEFAULT_STREAM  "http://liveradio.swr.de/sw282p3/swr3/play.mp3"
