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

    const std::vector<Station>& stations() const { return _stations; }

    /** Find by URL; returns nullptr if not present. */
    const Station* findByUrl(const char* url) const;

private:
    std::vector<Station> _stations;
};
