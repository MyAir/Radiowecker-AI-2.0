"""
patch_esp8266audio.py -- PlatformIO extra_script (pre:)

Fixes two compatibility issues between ESP8266Audio 1.9.9 and
Arduino-ESP32 3.0.0:

1. SPIFFS removed: AudioFileSourceFS.cpp and AudioOutputSPIFFSWAV.cpp use
   <SPIFFS.h> which no longer exists in Arduino-ESP32 3.x.  These files are
   not used in this project, so they are stubbed out (same technique as
   patch_lvgl.py uses for the ARM assembly files).

2. NetworkClient absent: ESP8266Audio 1.9.9 guards member declarations with
   ESP_ARDUINO_VERSION_MAJOR >= 3 and uses 'NetworkClient', but that type was
   never added to framework 3.0.0.  HTTPClient::getStreamPtr() returns
   WiFiClient* in this version, so NetworkClient is replaced with WiFiClient.

All patches are idempotent.
"""

import os
Import("env")  # noqa: F821  (SCons magic)

libdeps_src = os.path.join(
    env.subst("$PROJECT_DIR"),
    ".pio", "libdeps",
    env.subst("$PIOENV"),
    "ESP8266Audio", "src",
)

# ---------------------------------------------------------------------------
# 1. Stub out SPIFFS-dependent files (not used in this project)
# ---------------------------------------------------------------------------
SPIFFS_STUBS = [
    "AudioFileSourceFS.cpp",
    "AudioOutputSPIFFSWAV.cpp",
]
STUB = "/* stubbed out: SPIFFS not available in Arduino-ESP32 3.x */\n"

for filename in SPIFFS_STUBS:
    path = os.path.join(libdeps_src, filename)
    if not os.path.isfile(path):
        continue
    with open(path, "r") as fh:
        current = fh.read()
    if current.strip() == STUB.strip():
        continue  # already stubbed
    print("patch_esp8266audio.py: stubbing %s (SPIFFS not available)" % filename)
    with open(path, "w") as fh:
        fh.write(STUB)

# ---------------------------------------------------------------------------
# 2. Replace NetworkClient with WiFiClient (not present in framework 3.0.0)
# ---------------------------------------------------------------------------
NETWORK_PATCHES = [
    (
        "AudioFileSourceHTTPStream.h",
        "    NetworkClient client;",
        "    WiFiClient client;",
    ),
    (
        "AudioFileSourceHTTPStream.cpp",
        "NetworkClient *stream = http.getStreamPtr();",
        "WiFiClient *stream = http.getStreamPtr();",
    ),
    (
        "AudioFileSourceICYStream.cpp",
        "NetworkClient *stream = http.getStreamPtr();",
        "WiFiClient *stream = http.getStreamPtr();",
    ),
]

for filename, needle, replacement in NETWORK_PATCHES:
    path = os.path.join(libdeps_src, filename)
    if not os.path.isfile(path):
        continue
    with open(path, "r") as fh:
        content = fh.read()
    if needle not in content:
        continue  # already patched or pattern not found
    print("patch_esp8266audio.py: patching %s (NetworkClient -> WiFiClient)" % filename)
    with open(path, "w") as fh:
        fh.write(content.replace(needle, replacement, 1))
