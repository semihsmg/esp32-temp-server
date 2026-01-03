# ESP32 Environment Monitor

## Project Overview

Standalone temperature/humidity logger using ESP32 + DHT22. Stores data locally on LittleFS, serves web UI for live readings and CSV download. No external server dependencies.

## Architecture

```
DHT22 → ESP32 (60s interval) → LittleFS storage → Web UI
                                    ↓
                              Chunked CSV download
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
- ~13 bytes/entry → ~18 KB/day → ~110 days in 2MB

**Download format** (human-readable):
- Combined single CSV with header
- Row format: `YYYY-MM-DD,HH:MM,23.5,65.2`
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
| `/` | GET | Dashboard HTML with live readings |
| `/api/live` | GET | JSON: current temp/humidity |
| `/api/files` | GET | JSON: list of stored files with sizes |
| `/download` | GET | Stream combined CSV (all data) |
| `/api/delete` | POST | Delete all stored data files |

## Configuration Points

In `src/main.cpp`:
- `WIFI_SSID` / `WIFI_PASS` — Network credentials
- `GMT_OFFSET_SEC` — Timezone offset (10800 for Turkey/GMT+3)
- `DHT_PIN` — GPIO for sensor data (default: 4)
- `SAMPLE_INTERVAL_MS` — Reading interval (default: 60000)

## Dependencies (auto-installed by PlatformIO)

- `DHT sensor library` — Adafruit DHT driver
- `ESPAsyncWebServer` — Non-blocking web server
- `ArduinoJson` — JSON serialization for API responses
- `LittleFS` — Built into ESP32 Arduino core

## Known Limitations / Future Ideas

- No authentication on web UI
- No date range filtering on download (always exports all)
- Single sensor only (could expand to multiple DHT22s)
- No MQTT/external push option yet
- Web UI is embedded as raw string literal (could move to SPIFFS for easier editing)

## Build Commands

```bash
pio run                    # Build
pio run --target upload    # Flash to ESP32
pio device monitor         # Serial output (115200 baud)
```
