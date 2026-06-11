# Plane Radar Pro User Manual

Plane Radar Pro is an ESP32 touchscreen ADS-B radar that uses WiFi and the public adsb.fi OpenData API. It is not an SDR receiver and does not need an API key.

## Hardware

Supported target:

- ESP32-WROOM-32E 3.5-inch ST7796 touchscreen board
- XPT2046 resistive touch controller
- Optional UART GPS module such as NEO-6M / GY-GPS6MV2

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
| SD CS | GPIO5 |
| SD SCK | GPIO18 |
| SD MISO | GPIO19 |
| SD MOSI | GPIO23 |

## Optional GPS Wiring

The GPS module is optional. Without it, Plane Radar Pro uses the saved/manual home latitude and longitude.

| GPS module pin | ESP32 connection |
| --- | --- |
| VCC | 3.3V or 5V, depending on your module rating |
| GND | GND |
| TX | GPIO16 |
| RX | GPIO17, optional |

Default GPS serial settings:

- UART: UART2
- Baud: 9600
- RX: GPIO16
- TX: GPIO17

When GPS has a fresh fix, the radar automatically uses the GPS latitude and longitude as the home position. The top bar shows GPS status such as `No GPS`, `No fix`, or `Fix 8 sat`.

## Build And Upload

1. Open VS Code.
2. Open the folder `C:\Users\binsu\Documents\Plane Radar`.
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
- Selected range
- Last update or error status
- Radar rings
- Compass labels
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
| Reset WiFi | Hold 3 seconds to reset WiFi |
| Save | Save manual settings |

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

- Confirm GPS TX is wired to ESP32 GPIO16.
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
