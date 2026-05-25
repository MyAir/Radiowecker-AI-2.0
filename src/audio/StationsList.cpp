#include "StationsList.h"
#include <SD.h>
#include <ArduinoJson.h>
#include "../serial_safe.h"

static constexpr const char* STATIONS_FILE = "/stations.json";

size_t StationsList::load() {
    _stations.clear();

    if (!SD.exists(STATIONS_FILE)) {
        serial_safe_println("[Stations] /stations.json missing on SD");
        return 0;
    }
    File f = SD.open(STATIONS_FILE, FILE_READ);
    if (!f) {
        serial_safe_println("[Stations] cannot open /stations.json");
        return 0;
    }

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, f);
    f.close();
    if (err) {
        serial_safe_printf("[Stations] parse error: %s\n", err.c_str());
        return 0;
    }

    for (JsonObject obj : doc["stations"].as<JsonArray>()) {
        Station s;
        s.name     = (const char*)(obj["name"] | "");
        s.url      = (const char*)(obj["url"]  | "");
        s.favorite = obj["favorite"] | false;
        if (s.name.length() > 0 && s.url.length() > 0) {
            _stations.push_back(s);
        }
    }
    serial_safe_printf("[Stations] loaded %u station(s)\n", (unsigned)_stations.size());
    return _stations.size();
}

const StationsList::Station* StationsList::findByUrl(const char* url) const {
    if (!url) return nullptr;
    for (const auto& s : _stations) {
        if (s.url == url) return &s;
    }
    return nullptr;
}
