#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <DHT.h>
#include <ArduinoJson.h>
#include <time.h>

// ==================== CONFIGURATION ====================
const char* WIFI_SSID = "SMG";
const char* WIFI_PASS = "SMG*1234567*SMG";

#define DHT_PIN 4
#define DHT_TYPE DHT22
#define SAMPLE_INTERVAL_MS 60000  // 60 seconds
#define DATA_DIR "/data"

// NTP Configuration
const char* NTP_SERVER = "pool.ntp.org";
const long GMT_OFFSET_SEC = 10800;        // Adjust for your timezone (e.g., 3600 for GMT+1)
const int DAYLIGHT_OFFSET_SEC = 0;

// ==================== GLOBALS ====================
DHT dht(DHT_PIN, DHT_TYPE);
AsyncWebServer server(80);

float lastTemp = 0;
float lastHumidity = 0;
unsigned long lastSampleTime = 0;
bool timeInitialized = false;

// ==================== UTILITY FUNCTIONS ====================
String getDateFilename() {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) return "";
    char buf[16];
    strftime(buf, sizeof(buf), "%Y%m%d", &timeinfo);
    return String(DATA_DIR) + "/" + String(buf) + ".csv";
}

String getCurrentDate() {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) return "0000-00-00";
    char buf[11];
    strftime(buf, sizeof(buf), "%Y-%m-%d", &timeinfo);
    return String(buf);
}

String getCurrentTime() {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) return "0000";
    char buf[5];
    strftime(buf, sizeof(buf), "%H%M", &timeinfo);
    return String(buf);
}

String formatTimeForDisplay(const String& hhmm) {
    // Convert "1435" to "14:35"
    if (hhmm.length() != 4) return hhmm;
    return hhmm.substring(0, 2) + ":" + hhmm.substring(2, 4);
}

String formatDateForDisplay(const String& filename) {
    // Convert "20250103" to "2025-01-03"
    if (filename.length() < 8) return filename;
    return filename.substring(0, 4) + "-" + filename.substring(4, 6) + "-" + filename.substring(6, 8);
}

// ==================== DATA STORAGE ====================
void saveReading(float temp, float humidity) {
    if (!timeInitialized) return;
    
    String filename = getDateFilename();
    if (filename.isEmpty()) return;
    
    File file = LittleFS.open(filename, FILE_APPEND);
    if (!file) {
        // Try creating the directory
        LittleFS.mkdir(DATA_DIR);
        file = LittleFS.open(filename, FILE_APPEND);
        if (!file) {
            Serial.println("Failed to open file for writing");
            return;
        }
    }
    
    // Format: HHMM,235,652
    String line = getCurrentTime() + "," + 
                  String((int)(temp * 10)) + "," + 
                  String((int)(humidity * 10));
    
    file.println(line);
    file.close();
    
    Serial.println("Saved: " + line);
}

// ==================== WEB SERVER HANDLERS ====================
void handleRoot(AsyncWebServerRequest *request) {
    String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Environment Monitor</title>
    <style>
        * { box-sizing: border-box; margin: 0; padding: 0; }
        body { font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif; background: #1a1a2e; color: #eee; min-height: 100vh; padding: 20px; }
        .container { max-width: 600px; margin: 0 auto; }
        h1 { text-align: center; margin-bottom: 30px; color: #00d9ff; }
        .card { background: #16213e; border-radius: 12px; padding: 24px; margin-bottom: 20px; }
        .readings { display: grid; grid-template-columns: 1fr 1fr; gap: 20px; }
        .reading { text-align: center; }
        .reading .value { font-size: 48px; font-weight: bold; }
        .reading .label { font-size: 14px; color: #888; margin-top: 8px; }
        .temp .value { color: #ff6b6b; }
        .humidity .value { color: #4ecdc4; }
        .status { display: flex; justify-content: space-between; font-size: 12px; color: #666; margin-top: 16px; padding-top: 16px; border-top: 1px solid #333; }
        .actions { display: grid; grid-template-columns: 1fr 1fr; gap: 12px; }
        .btn { padding: 14px 20px; border: none; border-radius: 8px; font-size: 16px; cursor: pointer; transition: all 0.2s; }
        .btn-primary { background: #00d9ff; color: #000; }
        .btn-primary:hover { background: #00b8d9; }
        .btn-danger { background: #ff4757; color: #fff; }
        .btn-danger:hover { background: #ff3344; }
        .files { margin-top: 16px; }
        .file-list { font-size: 13px; color: #888; max-height: 150px; overflow-y: auto; }
        .file-item { padding: 6px 0; border-bottom: 1px solid #222; display: flex; justify-content: space-between; }
        .storage-info { font-size: 12px; color: #666; margin-top: 12px; }
    </style>
</head>
<body>
    <div class="container">
        <h1>🌡️ Environment Monitor</h1>
        
        <div class="card">
            <div class="readings">
                <div class="reading temp">
                    <div class="value" id="temp">--</div>
                    <div class="label">Temperature (°C)</div>
                </div>
                <div class="reading humidity">
                    <div class="value" id="humidity">--</div>
                    <div class="label">Humidity (%)</div>
                </div>
            </div>
            <div class="status">
                <span>Last update: <span id="lastUpdate">--</span></span>
                <span id="wifiStatus">●</span>
            </div>
        </div>
        
        <div class="card">
            <div class="actions">
                <a href="/download" class="btn btn-primary" id="downloadBtn">📥 Download CSV</a>
                <button class="btn btn-danger" id="deleteBtn" onclick="deleteData()">🗑️ Clear Data</button>
            </div>
            <div class="files">
                <div class="file-list" id="fileList">Loading...</div>
            </div>
            <div class="storage-info" id="storageInfo"></div>
        </div>
    </div>
    
    <script>
        async function fetchLive() {
            try {
                const res = await fetch('/api/live');
                const data = await res.json();
                document.getElementById('temp').textContent = data.temperature.toFixed(1);
                document.getElementById('humidity').textContent = data.humidity.toFixed(1);
                document.getElementById('lastUpdate').textContent = new Date().toLocaleTimeString();
                document.getElementById('wifiStatus').style.color = '#4ecdc4';
            } catch(e) {
                document.getElementById('wifiStatus').style.color = '#ff4757';
            }
        }
        
        async function fetchFiles() {
            try {
                const res = await fetch('/api/files');
                const data = await res.json();
                const list = document.getElementById('fileList');
                
                if (data.files.length === 0) {
                    list.innerHTML = '<div style="padding: 10px; text-align: center;">No data yet</div>';
                    document.getElementById('downloadBtn').style.opacity = '0.5';
                } else {
                    list.innerHTML = data.files.map(f => 
                        `<div class="file-item"><span>${f.date}</span><span>${(f.size/1024).toFixed(1)} KB</span></div>`
                    ).join('');
                    document.getElementById('downloadBtn').style.opacity = '1';
                }
                
                document.getElementById('storageInfo').textContent = 
                    `Storage: ${(data.usedBytes/1024).toFixed(1)} KB used / ${(data.totalBytes/1024).toFixed(0)} KB total`;
            } catch(e) {
                console.error(e);
            }
        }
        
        async function deleteData() {
            if (!confirm('Delete ALL stored data? This cannot be undone.')) return;
            try {
                const res = await fetch('/api/delete', { method: 'POST' });
                const data = await res.json();
                alert(`Deleted ${data.deleted} file(s)`);
                fetchFiles();
            } catch(e) {
                alert('Delete failed');
            }
        }
        
        fetchLive();
        fetchFiles();
        setInterval(fetchLive, 10000);
        setInterval(fetchFiles, 60000);
    </script>
</body>
</html>
)rawliteral";
    request->send(200, "text/html", html);
}

void handleLiveData(AsyncWebServerRequest *request) {
    JsonDocument doc;
    doc["temperature"] = lastTemp;
    doc["humidity"] = lastHumidity;
    doc["timestamp"] = getCurrentDate() + "T" + formatTimeForDisplay(getCurrentTime());
    
    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
}

void handleFileList(AsyncWebServerRequest *request) {
    JsonDocument doc;
    JsonArray files = doc["files"].to<JsonArray>();
    
    File root = LittleFS.open(DATA_DIR);
    if (root && root.isDirectory()) {
        File file = root.openNextFile();
        while (file) {
            if (!file.isDirectory()) {
                String name = file.name();
                // Extract date from filename (e.g., "20250103.csv" -> "2025-01-03")
                if (name.endsWith(".csv") && name.length() >= 12) {
                    String dateStr = name.substring(0, 8);
                    JsonObject f = files.add<JsonObject>();
                    f["date"] = formatDateForDisplay(dateStr);
                    f["filename"] = name;
                    f["size"] = file.size();
                }
            }
            file = root.openNextFile();
        }
    }
    
    doc["usedBytes"] = LittleFS.usedBytes();
    doc["totalBytes"] = LittleFS.totalBytes();
    
    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
}

// Chunked download state
struct DownloadState {
    std::vector<String> filenames;
    size_t fileIndex = 0;
    File currentFile;
    String currentDate;
    bool headerSent = false;
};

void handleDownload(AsyncWebServerRequest *request) {
    // Collect all CSV files
    std::vector<String> filenames;
    
    File root = LittleFS.open(DATA_DIR);
    if (root && root.isDirectory()) {
        File file = root.openNextFile();
        while (file) {
            if (!file.isDirectory()) {
                String name = file.name();
                if (name.endsWith(".csv")) {
                    filenames.push_back(String(DATA_DIR) + "/" + name);
                }
            }
            file = root.openNextFile();
        }
    }
    
    if (filenames.empty()) {
        request->send(404, "text/plain", "No data files found");
        return;
    }
    
    // Sort filenames (YYYYMMDD sorts chronologically)
    std::sort(filenames.begin(), filenames.end());
    
    // Create state for streaming
    DownloadState* state = new DownloadState();
    state->filenames = filenames;
    
    AsyncWebServerResponse *response = request->beginChunkedResponse("text/csv",
        [state](uint8_t *buffer, size_t maxLen, size_t index) -> size_t {
            size_t len = 0;
            char* buf = (char*)buffer;
            
            // Send header first
            if (!state->headerSent) {
                const char* header = "DateTime,Temperature,Humidity\n";
                size_t headerLen = strlen(header);
                if (headerLen <= maxLen) {
                    memcpy(buf, header, headerLen);
                    state->headerSent = true;
                    return headerLen;
                }
                return 0;
            }
            
            // Process files
            while (len < maxLen - 50 && state->fileIndex < state->filenames.size()) {
                // Open next file if needed
                if (!state->currentFile) {
                    String path = state->filenames[state->fileIndex];
                    state->currentFile = LittleFS.open(path, FILE_READ);
                    
                    if (!state->currentFile) {
                        state->fileIndex++;
                        continue;
                    }
                    
                    // Extract date from filename
                    int lastSlash = path.lastIndexOf('/');
                    String filename = path.substring(lastSlash + 1);
                    state->currentDate = formatDateForDisplay(filename.substring(0, 8));
                }
                
                // Read lines from current file
                while (state->currentFile.available() && len < maxLen - 50) {
                    String line = state->currentFile.readStringUntil('\n');
                    line.trim();
                    if (line.isEmpty()) continue;
                    
                    // Parse: HHMM,235,652
                    int comma1 = line.indexOf(',');
                    int comma2 = line.indexOf(',', comma1 + 1);
                    
                    if (comma1 > 0 && comma2 > comma1) {
                        String timeStr = formatTimeForDisplay(line.substring(0, comma1));
                        float temp = line.substring(comma1 + 1, comma2).toInt() / 10.0;
                        float humidity = line.substring(comma2 + 1).toInt() / 10.0;
                        
                        // Format output line
                        int written = snprintf(buf + len, maxLen - len,
                            "%s %s,%.1f,%.1f\n",
                            state->currentDate.c_str(),
                            timeStr.c_str(),
                            temp, humidity);
                        
                        if (written > 0 && (size_t)written < maxLen - len) {
                            len += written;
                        } else {
                            break; // Buffer full
                        }
                    }
                }
                
                // Move to next file if current is done
                if (!state->currentFile.available()) {
                    state->currentFile.close();
                    state->fileIndex++;
                }
            }
            
            // Cleanup when done
            if (len == 0 && state->fileIndex >= state->filenames.size()) {
                delete state;
                return 0; // Signal end of response
            }
            
            return len;
        }
    );
    
    response->addHeader("Content-Disposition", "attachment; filename=environment_data.csv");
    request->send(response);
}

void handleDelete(AsyncWebServerRequest *request) {
    int deleted = 0;
    
    File root = LittleFS.open(DATA_DIR);
    if (root && root.isDirectory()) {
        File file = root.openNextFile();
        std::vector<String> toDelete;
        
        while (file) {
            if (!file.isDirectory()) {
                toDelete.push_back(String(DATA_DIR) + "/" + file.name());
            }
            file = root.openNextFile();
        }
        
        for (const String& path : toDelete) {
            if (LittleFS.remove(path)) deleted++;
        }
    }
    
    JsonDocument doc;
    doc["deleted"] = deleted;
    doc["success"] = true;
    
    String response;
    serializeJson(doc, response);
    request->send(200, "application/json", response);
}

// ==================== SETUP & LOOP ====================
void setup() {
    Serial.begin(115200);
    Serial.println("\n\nEnvironment Monitor Starting...");
    
    // Initialize DHT
    dht.begin();
    
    // Initialize LittleFS
    if (!LittleFS.begin(true)) {
        Serial.println("LittleFS mount failed!");
        return;
    }
    Serial.printf("LittleFS: %d KB used / %d KB total\n", 
                  LittleFS.usedBytes() / 1024, 
                  LittleFS.totalBytes() / 1024);
    
    // Create data directory
    LittleFS.mkdir(DATA_DIR);
    
    // Connect to WiFi
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    Serial.print("Connecting to WiFi");
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 30) {
        delay(500);
        Serial.print(".");
        attempts++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\nConnected! IP: " + WiFi.localIP().toString());
        
        // Initialize NTP
        configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER);
        
        // Wait for time sync
        struct tm timeinfo;
        if (getLocalTime(&timeinfo, 10000)) {
            timeInitialized = true;
            Serial.println("Time synchronized");
        } else {
            Serial.println("Time sync failed!");
        }
    } else {
        Serial.println("\nWiFi connection failed!");
    }
    
    // Setup web server routes
    server.on("/", HTTP_GET, handleRoot);
    server.on("/api/live", HTTP_GET, handleLiveData);
    server.on("/api/files", HTTP_GET, handleFileList);
    server.on("/download", HTTP_GET, handleDownload);
    server.on("/api/delete", HTTP_POST, handleDelete);
    
    server.begin();
    Serial.println("Web server started on port 80");
    
    // Take first reading
    lastTemp = dht.readTemperature();
    lastHumidity = dht.readHumidity();
    if (!isnan(lastTemp) && !isnan(lastHumidity)) {
        saveReading(lastTemp, lastHumidity);
    }
}

void loop() {
    // Reconnect WiFi if needed
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("WiFi disconnected, reconnecting...");
        WiFi.reconnect();
        delay(5000);
        return;
    }
    
    // Sample sensor at interval
    if (millis() - lastSampleTime >= SAMPLE_INTERVAL_MS) {
        lastSampleTime = millis();
        
        float temp = dht.readTemperature();
        float humidity = dht.readHumidity();
        
        if (!isnan(temp) && !isnan(humidity)) {
            lastTemp = temp;
            lastHumidity = humidity;
            saveReading(temp, humidity);
            Serial.printf("Reading: %.1f°C, %.1f%%\n", temp, humidity);
        } else {
            Serial.println("DHT read failed!");
        }
    }
}
