# Weather Display

## 2026-05-21 — initial implementation

- `src/weather/WeatherManager.{h,cpp}` polls OpenWeatherMap **One Call 3.0**
  (`/data/3.0/onecall`) every 5 min over plain HTTP. Excludes `minutely`,
  `hourly`, `alerts`; uses ArduinoJson 7 `Filter` to keep only:
    - `current`: `temp`, `feels_like`, `weather[0].icon`, `weather[0].description`
    - `daily[0]`: `temp.morn`, `temp.day`, `feels_like.morn/day`, `pop`,
      `weather[0].icon`
    - `daily[1]`: same as daily[0] (used as "Morgen" tile)
- Slots: `current`, `morning` (today.morn), `afternoon` (today.day),
  `tomorrow` (daily[1].day).
- Config from SD `/weather.json`:
  `{"WeatherAPIKey","lat","lon","units","lang"}`. Refuses to start if key
  is the placeholder `"MyWeatherAPIKey"` or lat==lon==0.
- `WeatherManager.loop()` returns `true` on each successful poll —
  caller (`main.cpp`) calls `mainScreen.updateWeather(weather)`.

## 2026-05-21 — PNG icon rendering

- Enabled `LV_USE_LODEPNG=1` in `include/lv_conf.h` (auto-init via
  `lv_init()`/`lv_lodepng_init()`).
- `MainScreen` icon cache (`src/display/MainScreen.cpp`, anon namespace):
  reads `/assets/weather_icons/{code}.png` from SD into PSRAM once,
  builds an `lv_image_dsc_t` with `cf=LV_COLOR_FORMAT_RAW_ALPHA` and
  the raw PNG bytes in `.data`, then `lv_image_set_src(img, &dsc)`.
  LVGL's lodepng decoder identifies the PNG by magic and decodes to
  ARGB8888 on demand. Up to 20 codes cached (~60 KB in PSRAM).
- Tile labels updated: title row, icon left, big temperature right,
  sub line at bottom (`Gefühlt: NN°C` + description for current,
  `Regen: NN%` for forecast tiles).

## 2026-05-21 — UI: rename last tile

- "Nacht" tile renamed to "Morgen" (now shows `daily[1].day` for
  the next day instead of nighttime data).

## SD card layout required

- `/weather.json` — see template; user must set real lat/lon and API key.
- `/assets/weather_icons/{01d,01n,...,50d,50n}.png` — already shipped
  in `SD-Data/assets/weather_icons/`.
