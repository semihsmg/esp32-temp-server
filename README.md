# ESP32 Environment Monitor

Standalone temperature/humidity logger with web interface. Stores ~110 days of data at 60-second intervals.

## Hardware Required

- ESP32 dev board (any variant)
- DHT22 temperature/humidity sensor
- Jumper wires

## Wiring

```
DHT22          ESP32
┌─────┐        
│  +  │───────► 3.3V
│ OUT │───────► GPIO4
│  -  │───────► GND
└─────┘

Note: Most DHT22 modules have built-in pull-up resistor.
      If using bare sensor, add 10kΩ between OUT and 3.3V.
```

## Configuration

Edit these lines in `src/main.cpp`:

```cpp
const char* WIFI_SSID = "YOUR_WIFI_SSID";
const char* WIFI_PASS = "YOUR_WIFI_PASSWORD";

// Timezone offset in seconds (e.g., 10800 for GMT+3 Turkey)
const long GMT_OFFSET_SEC = 0;
```

## Build & Upload

### Using PlatformIO CLI

```bash
# Build
pio run

# Upload
pio run --target upload

# Monitor serial output
pio device monitor
```

### Using PlatformIO IDE (VS Code)

1. Install PlatformIO extension
2. Open this folder
3. Click "Upload" in bottom toolbar

## Usage

1. After upload, open Serial Monitor (115200 baud)
2. Note the IP address printed after WiFi connects
3. Open that IP in your browser

## Web Interface

| Endpoint | Description |
| -------- | ----------- |
| `/` | Dashboard with live readings |
| `/api/live` | JSON: current temperature/humidity |
| `/api/files` | JSON: list of stored data files |
| `/download` | Download all data as combined CSV |
| `/api/delete` | POST: delete all stored data |

## Data Format

### On Device (storage-optimized)

```
Filename: /data/20250103.csv
Content: HHMM,temp*10,humidity*10

Example:
1435,235,652   (14:35, 23.5°C, 65.2%)
```

### Downloaded CSV

```csv
DateTime,Temperature,Humidity
2025-01-03 14:35,23.5,65.2
2025-01-03 14:36,23.6,65.1
```

## Storage Capacity

- ~18 KB per day (60-second sampling)
- ~2 MB usable LittleFS storage
- **~110 days of continuous logging**

## Troubleshooting

| Issue | Solution |
| ----- | -------- |
| DHT read failed | Check wiring, try different GPIO |
| WiFi won't connect | Verify credentials, check 2.4GHz network |
| Time not syncing | Check internet connectivity |
| No data files | Wait 60 seconds for first sample |

## License

MIT - do whatever you want with it.
