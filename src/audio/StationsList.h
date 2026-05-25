#pragma once
#include <Arduino.h>
#include <vector>

/**
 * StationsList — reads /stations.json from the SD card.
 *
 * Schema:
 *   { "stations": [ { "name":"...", "url":"...", "favorite":bool }, ... ] }
 */
class StationsList {
public:
    struct Station {
        String name;
        String url;
        bool   favorite = false;
    };

    /** Load /stations.json from SD. Returns number of stations parsed. */
    size_t load();

    /** Persist current list to /stations.json. Returns true on success. */
    bool save();

    const std::vector<Station>& stations() const { return _stations; }

    /** Find by URL; returns nullptr if not present. */
    const Station* findByUrl(const char* url) const;

    // ---- CRUD (in-RAM; call save() to persist) -------------------------
    void addStation(const Station& s)                { _stations.push_back(s); }
    void updateStation(size_t index, const Station& s) {
        if (index < _stations.size()) _stations[index] = s;
    }
    void removeStation(size_t index) {
        if (index < _stations.size()) _stations.erase(_stations.begin() + index);
    }

private:
    std::vector<Station> _stations;
};
