lets refine the alarm framework:

# Alarm features

- Each alarm can either play a mp3 from the SD card or a web stream from stations.json. Playing web stream is the default.
- I want a single alarm and also repeated alarms. 
- The repeated alarms should be set per weekday. Example: one alarm every tuesday and wedensday and another monday, thursday and friday. 
- The weekdays for the alarm will be chosen in the alarm setup screen. 
- There is no limit how many alarms can be created and these are stored in alarms.json on the sd card. 
- Automatically create alarm.json on the SD card if none exists with a few sample alarms. 
- Create an example alarm.json SD-Data folder.

# Alarm functions on the main screen.

- The main screen shows the next scheduled alarm. 
- The "next" button skips the upcoming alarm and goes to the next one. 
- The "Prev" button reactivates the skipped alarms until they are at the latest for the current date/time.
- The alarm toggle button enables and disables every scheduled alarm.

# Alarm setup screen

- There should be an alarm setup screen accessed via the Alarms button on the settings screen. 
- In the alarm screen I want to CRUD alarms either single alarms or repeated per weekday. 
- The alarms have a title that is auto populated and editable via on screen keyboard.
- There is a drop down menu where i either choose the MP3 file from the SD or the radio station from stations.json, depending on the choice what type of sound is played.
- There should be a preview button and a volume slider to set the volume of the alarm.
- The settings for the alarms are maintained on alarms.json on the SD card.
- The alarm setup screen can have a brighter theme. 
- Create 3 mockup images for me to choose from.

# Alarm Screen

- When a alarm fires a alarm screen is displayed.
- The alarm screen should show the alarm title, current date and time.
- Below that it shall show the ID3 tags from the MP3 or the data from the web stream.
- There should be a Snooze and Stop button. 
- Snooze stops the sound but keeps the screen active.
- The Stop button stops the alarm and goes back to the main screen.
- The current weather should be shown as well as the morning, evening and tomorrows weather, includinng the weather icons. The chances for rain or snow should also be shown where applicable.
- There is already existing code to obtain and update the weather data from openweathermap.org
- The sensor readings and status bar are not needed on the alarm screen.
- The alarm screen has a brighter color scheme.
- Create 3 mockup images for me to choose from.
