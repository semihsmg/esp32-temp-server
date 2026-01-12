# ESP32 Environment Monitor

## Project Overview

Standalone temperature/humidity logger using ESP32 + DHT22. Stores data locally on LittleFS, serves web UI for live readings and CSV download. No external server dependencies.

## Architecture

```
DHT22 → ESP32 (5min interval) → LittleFS storage → Web UI
                                    ↓                      ↓
                              Chunked CSV download    Chart.js graph
```

## Key Files

- `src/main.cpp` — All logic: sensor reading, storage, web server, NTP sync
- `platformio.ini` — Build config and dependencies
- `README.md` — User-facing documentation with wiring

## Data Format Decisions

**Storage format** (optimized for space):
- Filename: `/data/YYYYMMDD.csv`
- Row format: `HHMM,temp*10,humidity*10` (no newline after humidity)
- Example: `1435,235,652` means 14:35, 23.5°C, 65.2%
- ~13 bytes/entry → ~3.6 KB/day (5min interval) → ~550 days in 2MB

**Download format** (human-readable):
- Combined single CSV with header
- Row format: `YYYY-MM-DD HH:MM,23.5,65.2` (DateTime with space separator)
- Chunked streaming to avoid RAM exhaustion

## Technical Decisions

1. **One CSV per day** — Date in filename, not in each row (saves ~6 bytes/entry)
2. **Integers × 10** — DHT22 only has 0.1 resolution anyway, saves decimal point bytes
3. **Chunked download** — Uses `beginChunkedResponse` callback, constant ~2-4KB RAM regardless of data size
4. **Separate delete button** — Safer than auto-delete after download (network failures)
5. **NTP required** — Won't save data until time is synced (prevents garbage timestamps)

## Web Endpoints

| Route | Method | Purpose |
|-------|--------|---------|
| `/` | GET | Dashboard HTML with live readings and history chart |
| `/api/live` | GET | JSON: current temp/humidity |
| `/api/files` | GET | JSON: list of stored files with sizes |
| `/api/history` | GET | JSON: all historical data (client-side filtering) |
| `/download` | GET | Stream combined CSV (all data) |
| `/api/delete` | POST | Delete all stored data files |

## Configuration Points

In `src/main.cpp`:
- `WIFI_SSID` / `WIFI_PASS` — Network credentials
- `GMT_OFFSET_SEC` — Timezone offset (10800 for Turkey/GMT+3)
- `DHT_PIN` — GPIO for sensor data (default: 4)
- `SAMPLE_INTERVAL_MS` — Reading interval (default: 300000 / 5 minutes)

## Dependencies (auto-installed by PlatformIO)

- `DHT sensor library` — Adafruit DHT driver
- `mathieucarbou/ESPAsyncWebServer` — Non-blocking web server (maintained fork)
- `ArduinoJson` — JSON serialization for API responses
- `LittleFS` — Built into ESP32 Arduino core

## Chart Feature

- **Library**: Chart.js loaded from CDN (zero flash usage)
- **Type**: Dual-axis line chart (temperature left, humidity right)
- **Ranges**: 24h / 7d / 30d selectable buttons (instant switch, client-side filtering)
- **Data caching**: All data fetched once, cached in browser, filtered client-side
- **Decimation**: Client-side LTTB algorithm, ~500 points max for performance
- **Auto-refresh**: Full data refresh every 5 minutes

## Known Limitations / Future Ideas

- No authentication on web UI
- No date range filtering on download (always exports all)
- Single sensor only (could expand to multiple DHT22s)
- No MQTT/external push option yet
- Web UI is embedded as raw string literal (could move to SPIFFS for easier editing)
- Chart requires internet connectivity to load Chart.js from CDN

## Build Commands

```bash
pio run                    # Build
pio run --target upload    # Flash to ESP32
pio device monitor         # Serial output (115200 baud)
```
