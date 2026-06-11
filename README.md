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
- IRQ not connected in the vendor demo
- Touch shares LCD SPI: SCK GPIO14, MOSI GPIO13, MISO GPIO12

SD pins are reserved in `include/config.h` but not used by the current firmware.

Optional GPS:

- Module type: NEO-6M/GY-GPS6MV2 style UART GPS
- GPS VCC to board 3.3V or 5V according to your module rating
- GPS GND to GND
- GPS TX to ESP32 GPIO16 (`GPS_RX`)
- GPS RX to ESP32 GPIO17 (`GPS_TX`, optional)
- Default baud: 9600

If GPS is connected and has a fresh fix, Plane Radar Pro automatically uses the GPS latitude/longitude as the radar home position. Without a GPS fix, it keeps using the saved/manual home coordinates.

## Calibration

Display rotation and touch calibration defaults live in `include/config.h` and are persisted after boot through `SettingsStore`.

If touches appear mirrored or offset for your board variant, adjust:

- `TOUCH_MIN_X`
- `TOUCH_MAX_X`
- `TOUCH_MIN_Y`
- `TOUCH_MAX_Y`
- `DEFAULT_ROTATION`
