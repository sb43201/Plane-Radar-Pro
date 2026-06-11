# Plane Radar Pro User Manual

Plane Radar Pro is an ESP32 touchscreen ADS-B radar that uses WiFi and the public adsb.fi OpenData API. It is not an SDR receiver and does not need an API key.

## Hardware

Supported target:

- ESP32-WROOM-32E 3.5-inch ST7796 touchscreen board
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
| LCD Backlight | GPIO27 |
| Touch CS | GPIO33 |
| Touch IRQ | GPIO36 |
| Touch SCK | GPIO14 |
| Touch MOSI | GPIO13 |
| Touch MISO | GPIO12 |
| Battery voltage ADC | GPIO34 |
| GPS data input | Expand input connector IO39 |
| SD CS | GPIO5 |
| SD SCK | GPIO18 |
| SD MISO | GPIO19 |
| SD MOSI | GPIO23 |

This pinout follows the vendor specification plus the Arduino, MicroPython, and ESP-IDF demo code. The uploaded schematic labels appear inconsistent with those sources, so the firmware uses the spec/demo-code wiring.

## Optional GPS Wiring

The GPS module is optional. Without it, Plane Radar Pro uses the saved/manual home latitude and longitude. Use the board's 2-pin expand input connector, not the `IO3` / `IO1` serial-port connector.

| GPS module pin | ESP32 connection |
| --- | --- |
| VCC | 3.3V or 5V, depending on your module rating |
| GND | GND |
| TX | 2-pin expand input connector `IO39` |
| RX | Leave unconnected |

Default GPS data settings:

- Data input pin: GPIO39
- Baud: 9600
- ESP32 output to GPS: disabled (`GPS_TX = -1`)

The expand input connector exposes `IO35` and `IO39`; both are input-only pins, which is fine because GPS only needs to send NMEA data to the ESP32. The firmware default uses `IO39`. If your connector wiring is easier on `IO35`, change `GPS_RX` to `35` in `include/config.h`.

When GPS has a fresh fix, the radar automatically uses the GPS latitude and longitude as the home position. The top bar shows GPS status such as `No GPS`, `No fix`, or `Fix 8 sat`.

## Advanced Radar Features

Aircraft trails:

- The radar keeps the last 20 known positions for each aircraft.
- Trails are drawn behind the aircraft icon so movement direction is easier to see.

Airport overlays:

- The radar marks nearby airports when they fit inside the selected range.
- Current overlays: `IND`, `HUF`, and `MQJ`.

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
6. Enter the hotspot password and save.
7. The ESP32 saves credentials and restarts into radar mode.

Future boots retry the saved hotspot for up to 60 seconds before opening setup mode again.

## Reset WiFi

To clear saved WiFi credentials:

1. Tap `Setup`.
2. Press and hold `Reset WiFi` for 3 seconds.
3. The ESP32 clears saved WiFi credentials.
4. The ESP32 restarts and reopens `PlaneRadar-Setup`.

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
| Cal Touch | Start four-point touchscreen calibration |
| Reset WiFi | Hold 3 seconds to reset WiFi |
| Save | Save manual settings |

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

- Confirm the board is the ESP32-WROOM-32E ST7796 model.
- Confirm the firmware pinout matches this manual.
- Try another USB cable and power source.

If touch is offset:

- Edit touch calibration constants in `include/config.h`.
- Rebuild and upload.

If GPS shows `No GPS`:

- Confirm GPS TX is wired to the 2-pin expand input connector `IO39`, or to the pin configured as `GPS_RX`.
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
