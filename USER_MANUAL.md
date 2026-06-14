# Plane Radar Pro User Manual

Plane Radar Pro is an ESP32 touchscreen ADS-B radar that uses WiFi and the public adsb.fi OpenData API. It is not an SDR receiver and does not need an API key.

## Hardware

Supported target:

- LCDWiki ESP32-WROOM-32E 2.8-inch ILI9341 touchscreen board
- XPT2046 resistive touch controller
- Optional NMEA GPS receiver module such as NEO-6M / GY-GPS6MV2

Board pinout used by this firmware:

| Function | ESP32 GPIO |
| --- | --- |
| LCD CS | GPIO15 |
| LCD DC/RS | GPIO2 |
| LCD SCK | GPIO14 |
| LCD MOSI | GPIO13 |
| LCD MISO | GPIO12 |
| LCD Backlight | GPIO21 |
| Touch CS | GPIO33 |
| Touch IRQ | GPIO36 |
| Touch SCK | GPIO25 |
| Touch MOSI | GPIO32 |
| Touch MISO | GPIO39 |
| Battery voltage ADC | GPIO34 |
| GPS data input | Expand pin IO35 |
| SD CS | GPIO5 |
| SD SCK | GPIO18 |
| SD MISO | GPIO19 |
| SD MOSI | GPIO23 |

This branch follows the LCDWiki 2.8-inch ESP32-32E display pin assignment for the E32R28T ILI9341/XPT2046 board.

## Optional GPS Wiring

The GPS module is optional. Without it, Plane Radar Pro uses the saved/manual home latitude and longitude. Use the board's `IO35` expand input pin, not the `IO3` / `IO1` serial-port connector.

| GPS module pin | ESP32 connection |
| --- | --- |
| VCC | 3.3V or 5V, depending on your module rating |
| GND | GND |
| TX | Expand input pin `IO35` |
| RX | Leave unconnected |

Default GPS data settings:

- Data input pin: GPIO35
- Baud: 9600
- ESP32 output to GPS: disabled (`GPS_TX = -1`)

The 2.8-inch board exposes `IO35` as an input-only expand pin, which is fine because GPS only needs to send NMEA data to the ESP32. The firmware default uses `IO35`.

When GPS has a fresh fix, the radar automatically uses the GPS latitude and longitude as the home position. The top bar shows GPS status such as `No GPS`, `No fix`, or `Fix 8 sat`.

## Advanced Radar Features

Scope-style radar:

- This branch defaults to portrait orientation on the 240x320 screen.
- Open `Setup` and tap `Scope` / `Radar` to switch between the dark circular scope and a lighter conventional radar style.
- The selected screen mode is saved in ESP32 Preferences/NVS.
- The radar screen uses a dark circular display with green range rings and crosshairs.
- Compass labels and range text are drawn around the scope.
- Aircraft labels show flight, aircraft type/category, and altitude next to the icon.

Aircraft trails:

- The radar keeps the last 20 known positions for each aircraft.
- Trails are drawn behind the aircraft icon so movement direction is easier to see.
- Aircraft outside the selected range, but within `EDGE_MARKER_RANGE_MULTIPLIER`, are drawn as red markers on the outer ring with distance labels.

Airport overlays:

- Airport overlays are loaded from SD card `/airports.csv`.
- The CSV format follows OurAirports columns.
- The firmware reads the file line by line and keeps only airports within `150 km`.
- The nearest `50` airports are kept in memory.
- Airport markers are small blue circles.
- Airport labels use this priority: IATA, GPS code, local code, ident.
- Labels are drawn only inside the current radar range and selected label distance.
- If the SD card or `airports.csv` is missing, Plane Radar Pro shows a warning but continues running.

Aircraft type:

- Plane Radar Pro parses ADS-B aircraft type from API fields `t` or `type` when available.
- The aircraft list and detail page show the type, such as `B739`, beside the flight/hex information.

Alerts:

- Close aircraft alert: within `ALERT_DISTANCE_KM`, default `2.0 km`.
- Low altitude alert: below `ALERT_LOW_ALT_FT`, default `3000 ft`.
- Alert thresholds are configured in `include/config.h`.

GPS compass:

- When GPS is present and reports course-over-ground, the radar shows a small compass readout such as `NE 045 deg`.
- This is a movement-based GPS course, not a magnetic compass, so it may be blank or unstable when the device is stationary.

GPS logging:

- GPS logging is off by default.
- Open `Setup` and tap `Log Off` to turn logging on. The button changes to `Log On`.
- The setting is saved in ESP32 Preferences/NVS.
- Logs are saved to SPIFFS as CSV at `/gps_log.csv`.
- A row is written every `GPS_LOG_INTERVAL_MS`, default `10000 ms`, only when GPS has a valid fix.
- The log rotates when it reaches `GPS_LOG_MAX_BYTES`, default `262144 bytes`.

CSV columns:

```text
millis,local_time,lat,lon,speed_kmph,course_deg,satellites
```

## Battery Voltage

Plane Radar Pro reads the board battery voltage sense line on `IO34` and shows it in the radar header as `Bat x.xxV`. The schematic shows BAT+ feeding `BAT_ADC` through a 100k/100k divider, so the ESP32 ADC sees half of the actual battery voltage.

The default voltage divider scale is:

```cpp
BATTERY_ADC_DIVIDER = 2.0f
```

If the displayed voltage is different from a multimeter reading, adjust `BATTERY_ADC_DIVIDER` in `include/config.h`, rebuild, and upload.

## Build And Upload

1. Open VS Code.
2. Open the cloned `Plane-Radar-Pro` project folder.
3. Click the PlatformIO icon.
4. Use `Project Tasks > esp32dev > General > Build`.
5. Use `Project Tasks > esp32dev > General > Upload`.
6. Use `Project Tasks > esp32dev > Platform > Monitor` or `General > Monitor` at 115200 baud.

The PlatformIO board is:

```ini
board = esp32dev
```

If upload does not find the board automatically, check Windows Device Manager for the CH340 USB serial COM port and add this to `platformio.ini`:

```ini
upload_port = COM5
monitor_port = COM5
```

Replace `COM5` with your actual port.

## Phone Hotspot Setup

On first boot, or when saved WiFi cannot connect, the ESP32 opens a setup network:

```text
PlaneRadar-Setup
```

The screen shows:

```text
Connect phone to WiFi:
PlaneRadar-Setup
Then open: 192.168.4.1
```

Setup flow:

1. Turn on your phone hotspot.
2. Power on Plane Radar Pro.
3. If it does not connect automatically, connect your phone to `PlaneRadar-Setup`.
4. Open `192.168.4.1` in your phone browser.
5. Select your phone hotspot SSID.
6. Enter the hotspot password.
7. Optional: enter a center airport code such as `IND` or `KIND`.
8. Optional: enter manual home latitude and longitude.
9. Save.

The captive portal center airport and home latitude/longitude fields are optional. If an airport code is entered, Plane Radar Pro uses that airport as the radar center without overwriting the manual home latitude/longitude. If latitude/longitude are entered instead, Plane Radar Pro switches to Manual center mode. If GPS has a fix, you can select GPS center mode later from `Setup`.
10. The ESP32 saves credentials and restarts into radar mode.

Future boots try saved networks by priority, using up to 30 seconds per attempt, before opening setup mode. If no WiFi is available and nobody completes setup, the setup portal closes after about 2 minutes and Plane Radar Pro continues to the radar screen offline. You can reopen setup later from `Setup > WiFi > Add`.

Phone hotspot notes:

- ESP32 WiFi is 2.4 GHz only. If your phone hotspot is 5 GHz only, it will not appear in the setup portal.
- On iPhone, turn on `Maximize Compatibility` in Personal Hotspot.
- On Android, set the hotspot AP band to `2.4 GHz` when that option is available.
- Keep the hotspot screen open while scanning if your phone sleeps or disables discovery.
- Some phones cannot broadcast a hotspot while also connected to `PlaneRadar-Setup`. In that case, use two devices:
  1. Phone A: turn on hotspot.
  2. Phone/laptop B: connect to `PlaneRadar-Setup`.
  3. On Phone/laptop B, open `192.168.4.1`.
  4. Select Phone A's hotspot SSID and save.

## Switching Between Home WiFi And Phone Hotspot

Plane Radar Pro supports up to 10 saved WiFi networks. Each saved network has:

- SSID
- Password, stored but not displayed
- Priority order
- Enabled/disabled state

At boot, Plane Radar Pro loads the saved network list, sorts it by priority, and tries enabled networks in order. It tries each network for up to 30 seconds and retries before moving to the next one. If none connect, it opens the `PlaneRadar-Setup` captive portal. If the portal is not used within about 2 minutes, the device continues offline and keeps the UI usable.

Best no-reset method:

1. Save both your home WiFi and phone hotspot in WiFi Settings.
2. Put the network you prefer first in priority order.
3. Enable both networks.
4. When you are home, Plane Radar Pro connects to home WiFi.
5. When traveling, turn on the phone hotspot and Plane Radar Pro connects to the hotspot.

Alternative simple method:

Set your phone hotspot SSID and password to exactly match your home WiFi SSID and password. Then the ESP32 can use the same saved credentials for both.

## Multi-WiFi Support

Open `Setup`, then tap `WiFi` to open the WiFi Settings page.

The WiFi Settings page shows:

- Saved networks
- Priority number
- Enabled/disabled state
- Connected network
- IP address
- RSSI signal strength

Controls:

| Control | Action |
| --- | --- |
| Saved network row | Select network |
| Add | Opens `PlaneRadar-Setup` portal to add/update a network |
| Delete | Deletes the selected network |
| On/Off | Enables or disables the selected network |
| Up | Moves selected network higher priority |
| Dn | Moves selected network lower priority |
| Export | Saves `/wifi_config.json` to SD card |
| Import | Loads `/wifi_config.json` from SD card |
| Reset WiFi | Hold 3 seconds to clear all saved WiFi |

The `On/Off` button does not delete a network. It only enables or disables the selected saved network. Disabled networks keep their password but are skipped during startup and reconnect attempts.

Adding a network:

1. Tap `Setup`.
2. Tap `WiFi`.
3. Tap `Add`.
4. Connect your phone/laptop to `PlaneRadar-Setup`.
5. Open `192.168.4.1`.
6. Select the WiFi network or phone hotspot.
7. Enter the password.
8. Optional: enter a center airport code.
9. Optional: update the manual home latitude and longitude.
10. Save.

You do not need to disconnect from the current WiFi before adding a new home WiFi or phone hotspot. The new network is added to the saved list, and existing networks remain saved unless you delete them or disable them with `On/Off`.

Passwords are not shown on the ESP32 screen. They are stored in ESP32 Preferences/NVS and can be exported to SD card only if you choose `Export`.

SD backup file:

```text
/wifi_config.json
```

Example:

```json
{
  "networks": [
    {
      "ssid": "Bin-iPhone",
      "password": "xxxx",
      "priority": 1,
      "enabled": true
    }
  ]
}
```

Automatic reconnect:

- If WiFi drops while running, the UI stays active.
- Plane Radar Pro retries every 10 seconds.
- If the current SSID is unavailable, it tries other enabled saved networks.

## Reset WiFi

To clear saved WiFi credentials:

1. Tap `Setup`.
2. Tap `WiFi` if you want to manage the full network list, or use `Reset WiFi` from Setup.
3. Press and hold `Reset WiFi` for 3 seconds.
4. The ESP32 clears saved WiFi credentials.
5. The ESP32 restarts and reopens `PlaneRadar-Setup`.

## Radar Screen

The radar screen shows:

- Aircraft count
- WiFi status
- GPS status
- GPS compass/course when available
- Battery voltage
- Close or low-altitude alert banner
- Selected range
- Last update or error status
- Radar rings
- Compass labels
- Airport overlays
- Aircraft icons rotated by track heading
- Aircraft trails

Aircraft altitude colors:

| Color | Altitude |
| --- | --- |
| Red | Below 5000 ft or unknown |
| Yellow | 5000 to 20000 ft |
| Green | Above 20000 ft |

Aircraft marker shapes:

| Marker | Meaning |
| --- | --- |
| Rotated triangle | Aircraft position and track/heading |
| Circle around triangle | Contrast ring to make the aircraft visible, especially in daylight mode |
| Double/thicker circle | Selected aircraft |
| Red edge marker | Aircraft just outside the selected radar range |

The circle around an aircraft is not an alert by itself. Alerts are shown in the alert banner below the radar scope.

Tap an aircraft icon to open the aircraft detail page.

## Controls

Bottom buttons:

| Button | Action |
| --- | --- |
| Radar | Show radar screen |
| List | Show nearest aircraft list |
| Range | Cycle 10, 25, 50, 100 km |
| Day/Night | Toggle theme |
| Setup | Open settings screen |

Settings screen:

| Control | Action |
| --- | --- |
| Latitude +/- | Adjust manual home latitude |
| Longitude +/- | Adjust manual home longitude |
| Range Change | Cycle radar range |
| Theme | Toggle day/night mode |
| Scope / Radar | Toggle main radar display style |
| Airport Overlay | Toggle airport markers |
| Airport Label Distance | Cycle 10, 25, 50, 100, 150 km |
| Cal Touch | Start four-point touchscreen calibration |
| Log On / Log Off | Toggle GPS location logging |
| Center | Cycle radar center source: Manual, GPS, or APT |
| WiFi | Open multi-network WiFi Settings |
| Reset / Reset WiFi | Hold 3 seconds to reset WiFi |
| Save | Save manual settings |

Radar center modes:

| Mode | Meaning |
| --- | --- |
| Manual | Use the entered/saved latitude and longitude |
| GPS | Use the current live GPS fix when available |
| APT | Use the selected airport or airport code |

Tapping an airport and choosing `Center Radar Here` switches to APT center mode. This does not overwrite the saved manual latitude/longitude shown on the Setup page. To switch back, open `Setup` and tap `Center` until the desired mode is shown.

To save the current GPS fix as the default Manual home location, set `Center` to `GPS`, wait for a GPS fix, then tap `Save`. Plane Radar Pro copies the current GPS latitude/longitude into the saved Manual home position and switches `Center` back to `Manual`.

## Touchscreen Calibration

Touch calibration is built into the firmware.

1. Tap `Setup`.
2. Tap `Cal Touch`.
3. Tap each crosshair as it appears.
4. Release your finger between each tap.
5. After four points, calibration is saved to ESP32 flash/NVS.
6. The app returns to the Setup screen.

Calibration is preserved after reboot. If the result is poor, run `Cal Touch` again.

## WiFi Status

Top bar statuses:

| Status | Meaning |
| --- | --- |
| WiFi: Connected | Connected to hotspot/router |
| WiFi: Searching | Trying to connect or reconnect |
| WiFi: Setup Mode | Captive portal is open |

If WiFi disconnects while running, the radar screen stays visible, shows `WiFi lost`, and retries every 10 seconds.

## Troubleshooting

If the screen is blank:

- Confirm the board is the LCDWiki ESP32-WROOM-32E 2.8-inch ILI9341 model.
- Confirm the firmware pinout matches this manual.
- Try another USB cable and power source.

If touch is offset:

- Edit touch calibration constants in `include/config.h`.
- Rebuild and upload.

If GPS shows `No GPS`:

- Confirm GPS TX is wired to expand input pin `IO35`, or to the pin configured as `GPS_RX`.
- Confirm GPS GND is connected to ESP32 GND.
- Move the antenna near a window or outdoors.
- Wait several minutes for first fix.

If GPS shows `No fix`:

- GPS serial data is arriving, but satellites are not locked yet.
- Place the antenna with a clear sky view.

If WiFi setup does not appear:

- Hold `Reset WiFi` for 3 seconds from the Setup screen.
- Restart the board.
- Connect to `PlaneRadar-Setup` and open `192.168.4.1`.
