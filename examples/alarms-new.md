# Alarm features

- Each alarm plays either an SD MP3 or a web stream from stations.json (default: web stream).
- Alarms can be one-time or repeated per weekday (days selected in the alarm setup screen).
- Unlimited alarms stored in alarms.json on the SD card; auto-create with sample alarms if missing.
- Add an example alarms.json to the SD-Data/ folder.

# Alarm functions on the main screen

- Shows the next scheduled alarm.
- **Next** button skips the upcoming alarm and advances to the next one.
- **Prev** button reactivates skipped alarms back to the current date/time.
- Alarm toggle button enables/disables all scheduled alarms.

# Alarm setup screen

- Accessed via the Alarms button on the settings screen.
- Full CRUD for one-time and weekday-repeated alarms.
- Auto-populated, editable alarm title (on-screen keyboard).
- Dropdown to choose an SD MP3 or a radio station from stations.json.
- Preview button and volume slider.
- Brighter theme.
- Create 3 mockup images to choose from.

# Alarm screen

- Shows alarm title, current date/time, and ID3 tags (MP3) or stream metadata (web radio).
- Snooze and Stop buttons; Snooze silences audio but keeps the screen active; Stop ends the alarm and returns to the main screen.
- Weather is shown directly on the alarm screen (not a separate screen): current conditions plus morning, evening, and tomorrow forecasts — with icons and rain/snow probability where applicable (uses existing OpenWeatherMap integration).
- No sensor readings or status bar.
- Brighter color scheme.
- Create 3 mockup images to choose from.
