# Plane Radar Pro

PlatformIO Arduino firmware for an ESP32-WROOM-32E 3.5-inch ST7796/XPT2046 touchscreen board.

Plane Radar Pro is an internet ADS-B radar client. It uses WiFi and the public adsb.fi OpenData API; it is not an SDR receiver and does not require an API key.

## Build

```powershell
pio run
pio run -t upload
pio device monitor -b 115200
```

On first boot, connect to the `PlaneRadarPro-Setup` captive portal and choose WiFi. Home latitude/longitude, range, theme, rotation, and touch calibration are stored in ESP32 Preferences/NVS.

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

## Calibration

Display rotation and touch calibration defaults live in `include/config.h` and are persisted after boot through `SettingsStore`.

If touches appear mirrored or offset for your board variant, adjust:

- `TOUCH_MIN_X`
- `TOUCH_MAX_X`
- `TOUCH_MIN_Y`
- `TOUCH_MAX_Y`
- `DEFAULT_ROTATION`
