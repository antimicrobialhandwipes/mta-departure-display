# MTA Departure Display

A desktop-to-microcontroller pipeline that shows live N train departure times on a small e-paper display. A Flask server on your computer polls the MTA's real-time GTFS feed and parses it into a tiny JSON payload; an ESP32 fetches that payload over Wi-Fi and renders it on an EPD (electronic paper display).

```
MTA GTFS-Realtime Feed
        │
        ▼
  server/main.py   (Flask, runs on your computer)
        │  GET /subway → { "manhattan": [...], "coney": [...] }
        ▼
  client/client.ino (ESP32 + e-paper display)
```

## How it works

1. **Server** (`server/main.py`) — polls the MTA's N/Q/R/W GTFS-realtime feed via [`nyct_gtfs`](https://pypi.org/project/nyct-gtfs/), filters trips for two stops (Manhattan-bound and Coney Island-bound N train), and computes minutes-until-arrival for each. It exposes this as a small JSON API at `/subway`.
2. **Client** (`client/client.ino`) — an ESP32 sketch that connects to Wi-Fi, polls the Flask server's `/subway` endpoint, parses the JSON with ArduinoJson, and draws the formatted departure times ("4 min, 17 min, ...") to a 2.13" SSD1680 e-paper display.

## Repo structure

```
mta-departure-display/
├── client/
│   ├── client.ino          # ESP32 sketch: Wi-Fi, HTTP fetch, EPD rendering
│   └── libraries/          # E-paper display driver (SSD1680) + fonts
│       ├── EPD.cpp / EPD.h
│       ├── EPD_Init.cpp / EPD_Init.h
│       ├── EPDfont.h
│       ├── Pic.h
│       ├── spi.cpp / spi.h
└── server/
    ├── main.py              # Flask server: fetches + parses MTA data
    └── reference.py         # (reserved / notes)
```

## Setup

### Server

**Requirements:** Python 3, `flask`, `nyct_gtfs`

```bash
pip install flask nyct_gtfs
python server/main.py
```

The server starts on `0.0.0.0:5000`, so it's reachable from other devices on your network (like the ESP32) at `http://<your-computer-ip>:5000/subway`.

To have it launch automatically when your computer starts, add it to your OS's startup/login items (e.g. Task Scheduler on Windows, a LaunchAgent on macOS, or a systemd service on Linux).

### Client (ESP32)

1. Open `client/client.ino` in the Arduino IDE (with ESP32 board support installed).
2. Install the `ArduinoJson` library.
3. Update the config block at the top of the file:
   ```cpp
   const char* WIFI_SSID = "your-network";
   const char* WIFI_PASS = "your-password";
   const char* PROXY_URL = "http://<your-computer-ip>:5000/subway";
   ```
4. Flash the sketch to your ESP32. On boot, it connects to Wi-Fi, fetches the current departure times, and draws them to the e-paper display.

## API

`GET /subway`

```json
{
  "manhattan": [4, 17, 23, 37],
  "coney": [6, 14, 29, 44]
}
```

Each array is a sorted list of minutes-until-arrival (max 4 entries) for that direction.

## Notes

- The display only updates once, at ESP32 boot/reset — there's no polling loop yet, so refreshing the times currently means resetting the device.
- The e-paper driver code (`client/libraries/`) targets a 250×122 SSD1680-based display and is largely vendor-provided; most customization happens in `client.ino`.
