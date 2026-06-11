# Plane Radar Pro

PlatformIO Arduino firmware for an ESP32-WROOM-32E 3.5-inch ST7796/XPT2046 touchscreen board.

Plane Radar Pro is an internet ADS-B radar client. It uses WiFi and the public adsb.fi OpenData API; it is not an SDR receiver and does not require an API key.

## Build

```powershell
pio run
pio run -t upload
pio device monitor -b 115200
```

On first boot, connect your phone to the `PlaneRadar-Setup` captive portal and open `192.168.4.1`. Choose your phone hotspot SSID, enter the password, and the ESP32 will save the credentials in flash/NVS. Future boots retry the saved hotspot for up to 60 seconds before opening setup mode again.

To clear saved WiFi, open `Setup` on the touchscreen and hold `Reset WiFi` for 3 seconds. The ESP32 clears the saved credentials, restarts, and opens `PlaneRadar-Setup`.

## Hardware

LCD:

- CS GPIO15
- DC/RS GPIO2
- SCK GPIO14
- MOSI GPIO13
- MISO GPIO12
- BL GPIO27

Touch:

- CS GPIO33
- SCK GPIO14
- MOSI GPIO13
- MISO GPIO12
- IRQ GPIO36

Battery:

- Battery voltage ADC sense GPIO34
- Voltage scale is `BATTERY_ADC_DIVIDER = 2.0f` for the onboard 100k/100k BAT+ divider

This pinout follows the vendor specification plus the Arduino, MicroPython, and ESP-IDF demo code. The uploaded schematic labels appear inconsistent with those sources. SD pins are reserved in `include/config.h` but not used by the current firmware.

Optional GPS:

- Module type: NEO-6M/GY-GPS6MV2 style NMEA GPS receiver
- GPS VCC to board 3.3V or 5V according to your module rating
- GPS GND to GND
- GPS TX to the board expand input connector pin `IO39` (`GPS_RX`)
- GPS RX may be left unconnected (`GPS_TX = -1`)
- Default baud: 9600

Use the 2-pin expand input connector, not the board's `IO3`/`IO1` serial port. The expand input connector exposes `IO35` and `IO39`; both are input-only pins, which is fine because GPS only needs to send NMEA data to the ESP32. The firmware default uses `IO39`.

If GPS is connected and has a fresh fix, Plane Radar Pro automatically uses the GPS latitude/longitude as the radar home position. Without a GPS fix, it keeps using the saved/manual home coordinates.

The radar header also shows battery voltage from the board's `IO34` ADC sense line. The schematic shows a 100k/100k BAT+ divider, so the firmware multiplies the ADC reading by 2.0. If the displayed voltage does not match a multimeter reading, tune `BATTERY_ADC_DIVIDER` in `include/config.h`.

## Advanced Radar Features

- Radar screen uses a dark circular scope layout with green rings, crosshairs, compass labels, range marker, and stacked aircraft labels.
- Aircraft trails keep the last 20 positions per aircraft.
- Airport overlays mark `IND`, `HUF`, and `MQJ` when they are inside the selected radar range.
- Aircraft type labels are parsed from ADS-B field `t` or `type` when provided by the API.
- Alert banner shows aircraft within `ALERT_DISTANCE_KM` or below `ALERT_LOW_ALT_FT`.
- GPS compass shows course-over-ground when the optional GPS has a valid course fix.
- Optional GPS logging saves fixes to SPIFFS CSV at `/gps_log.csv`.

GPS logging is controlled from `Setup` with the `Log On` / `Log Off` button. It is off by default, logs only valid GPS fixes, writes every `GPS_LOG_INTERVAL_MS`, and rotates the file at `GPS_LOG_MAX_BYTES`.

## Calibration

Touch calibration is available on the device: tap `Setup`, then `Cal Touch`, then tap the four crosshairs. Calibration is saved to ESP32 Preferences/NVS.

Display rotation and touch calibration defaults live in `include/config.h`. If the touchscreen cannot be used well enough to reach `Cal Touch`, adjust these fallback constants and upload again:

- `TOUCH_MIN_X`
- `TOUCH_MAX_X`
- `TOUCH_MIN_Y`
- `TOUCH_MAX_Y`
- `DEFAULT_ROTATION`
