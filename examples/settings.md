# settings screen layout

- The settings screen is tabbed containing the current Alarm and System screen plus a new Debug screen.
- First tab is the Alarm settings screen. Name it "Alarm"
- Second tab is the "System" screen.
- Last there is a optional Debug screen depending on the debug flag in System setting.
- There is a back button in the top left corner bringing us back to main screen.

# System screen

- Has seperate brightness sliders for main screen, alarm screen and settings screen.
- Has option to set the snooze duration.
- Has option to set max alarm duration.
- Has option to set inactivity timeout duration.
- Has a Debug tickbox to enable the debug screen.
- Alarm test function currently on System screen is moved to debug screen.
- all settings are persisted on SD card in config.json. Autcreate if missing. Append new objects as necessary.

# debug screen

- is only visible if debug flag in system settings is on
- contains alarm test function from system screen

# alarm settings screen

- move the volume slider down a bit and make the test buttonns a bit taller.
- assure no UI element is lost due to new tab bar on top.