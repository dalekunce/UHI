#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <TinyGPSPlus.h>
#include <HardwareSerial.h>
#include <DHT.h>
#include <time.h>
#include <Adafruit_PM25AQI.h>
#include <esp_sleep.h>
#include <SPI.h>
#include <SD.h>
#include <WebServer.h>
#include <WiFiManager.h>
#include <ArduinoJson.h>

// ======= CONFIG - move secrets to src/secrets.cpp =======
#include "secrets.h"

// Mutable runtime config stored on SD card
struct Config {
  char mqtt_server[128];
  int mqtt_port;
  char mqtt_topic[128];
  // WiFi stays in secrets for simplicity; you could move into config as well
};

Config runtimeConfig;

bool loadConfig() {
  if (!SD.exists("/config.json")) return false;
  File f = SD.open("/config.json", FILE_READ);
  if (!f) return false;
  StaticJsonDocument<512> doc;
  DeserializationError err = deserializeJson(doc, f);
  f.close();
  if (err) return false;
  if (doc.containsKey("mqtt_server")) {
    strlcpy(runtimeConfig.mqtt_server, doc["mqtt_server"], sizeof(runtimeConfig.mqtt_server));
  } else {
    strlcpy(runtimeConfig.mqtt_server, MQTT_SERVER, sizeof(runtimeConfig.mqtt_server));
  }
  runtimeConfig.mqtt_port = doc.containsKey("mqtt_port") ? doc["mqtt_port"].as<int>() : MQTT_PORT;
  if (doc.containsKey("mqtt_topic")) {
    strlcpy(runtimeConfig.mqtt_topic, doc["mqtt_topic"], sizeof(runtimeConfig.mqtt_topic));
  } else {
    strlcpy(runtimeConfig.mqtt_topic, MQTT_TOPIC, sizeof(runtimeConfig.mqtt_topic));
  }
  return true;
}

bool saveConfig(const char* server, int port, const char* topic) {
  StaticJsonDocument<256> outdoc;
  outdoc["mqtt_server"] = server;
  outdoc["mqtt_port"] = port;
  outdoc["mqtt_topic"] = topic;
  File f = SD.open("/config.json", FILE_WRITE);
  if (!f) return false;
  if (serializeJson(outdoc, f) == 0) {
    f.close();
    return false;
  }
  f.close();
  // update runtimeConfig
  strlcpy(runtimeConfig.mqtt_server, server, sizeof(runtimeConfig.mqtt_server));
  runtimeConfig.mqtt_port = port;
  strlcpy(runtimeConfig.mqtt_topic, topic, sizeof(runtimeConfig.mqtt_topic));
  return true;
}

#define SD_CS_PIN 5 // default CS pin for SPI SD modules; change if needed

// DHT settings
#define DHTPIN 4      // GPIO where DHT is connected
#define DHTTYPE DHT22 // DHT11 or DHT22

// GPS UART
#define GPS_RX_PIN 16 // GPS TX -> ESP32 RX
#define GPS_TX_PIN 17 // GPS RX -> ESP32 TX (not used if GPS is TX-only)
#define GPS_BAUD 9600

// PM2.5 UART (use a second UART; change pins if needed)
#define PM_RX_PIN 26 // PM sensor TX -> ESP32 RX2
#define PM_TX_PIN 27 // PM sensor RX -> ESP32 TX2 (if required)
#define PM_BAUD 9600

// Publish interval in milliseconds
const unsigned long PUBLISH_INTERVAL = 10000UL;

// =============================================

WiFiClient espClient;
PubSubClient mqttClient(espClient);
TinyGPSPlus gps;
HardwareSerial GPSSerial(1);
DHT dht(DHTPIN, DHTTYPE);
HardwareSerial PMSerial(2);
Adafruit_PM25AQI pm25;

// Simple HTTP server to serve logs over HTTP when connected to WiFi
WebServer server(80);
bool httpServerStarted = false;

// Health / error tracking
bool wifi_ok = false;
bool mqtt_ok = false;
bool pm_ok = false;
bool dht_ok = false;
unsigned long lastGpsFixMillis = 0;
int pm_fail_count = 0;
const int PM_FAIL_THRESHOLD = 3;
int mqtt_backoff = 1; // seconds (exponential backoff)
unsigned long lastMqttAttemptMillis = 0;

unsigned long lastPublish = 0;

// UUID helpers: generate UUIDv4 and store/read from SD at /device_id.txt
static void generateUuidV4(char* out /* 37 bytes */) {
  uint8_t b[16];
  for (int i = 0; i < 4; ++i) {
    uint32_t r = esp_random();
    memcpy(&b[i * 4], &r, 4);
  }
  // set version to 4
  b[6] = (b[6] & 0x0F) | 0x40;
  // set variant to RFC4122
  b[8] = (b[8] & 0x3F) | 0x80;
  snprintf(out, 37,
           "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
           b[0], b[1], b[2], b[3], b[4], b[5], b[6], b[7], b[8], b[9], b[10], b[11], b[12], b[13], b[14], b[15]);
}

String getDeviceId() {
  const char* path = "/device_id.txt";
  // If SD not available, fallback to efuse MAC formatted
  if (!SD.begin(SD_CS_PIN)) {
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    char buf[18];
    snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return String(buf);
  }

  if (SD.exists(path)) {
    File f = SD.open(path, FILE_READ);
    if (f) {
      char buf[64] = {0};
      size_t r = f.readBytesUntil('\n', buf, sizeof(buf) - 1);
      f.close();
      if (r > 0) {
        // trim whitespace
        String s = String(buf);
        s.trim();
        if (s.length() > 0) return s;
      }
    }
  }

  // create a new UUID and store it
  char uuid[37] = {0};
  generateUuidV4(uuid);
  File f = SD.open(path, FILE_WRITE);
  if (f) {
    f.println(uuid);
    f.close();
  }
  return String(uuid);
}

void setupWiFi() {
  delay(10);
  Serial.print("Connecting to WiFi ");
  Serial.print(WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  int retries = 0;
  while (WiFi.status() != WL_CONNECTED && retries < 20) {
    delay(500);
    Serial.print(".");
    retries++;
  }
  Serial.println();
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("WiFi connected, IP: ");
    Serial.println(WiFi.localIP());
    // Initialize time via NTP
    configTime(0, 0, "pool.ntp.org", "time.nist.gov");
    // wait briefly for time to be set
    struct tm timeinfo;
    if (getLocalTime(&timeinfo, 5000)) {
      Serial.println("Time synchronized");
    } else {
      Serial.println("Failed to sync time");
    }
  } else {
    Serial.println("WiFi connection failed");
  }
}

void reconnectMQTT() {
  // Exponential backoff and non-blocking attempts
  if (mqttClient.connected()) {
    mqtt_ok = true;
    return;
  }
  unsigned long now = millis();
  if (now - lastMqttAttemptMillis < (unsigned long)mqtt_backoff * 1000) {
    // still waiting for next attempt
    mqtt_ok = false;
    return;
  }
  lastMqttAttemptMillis = now;
  Serial.print("Attempting MQTT connection (backoff ");
  Serial.print(mqtt_backoff);
  Serial.println("s)...");
  String clientId = "ESP32-" + String((uint32_t)ESP.getEfuseMac(), HEX);
  if (mqttClient.connect(clientId.c_str())) {
    Serial.println("MQTT connected");
    mqtt_ok = true;
    mqtt_backoff = 1; // reset backoff
  } else {
    Serial.print("MQTT connect failed, rc=");
    Serial.println(mqttClient.state());
    mqtt_ok = false;
    // increase backoff up to a cap
    mqtt_backoff = min(mqtt_backoff * 2, 64);
  }
}

// Ensure WiFi is connected; non-blocking short attempt
bool ensureWiFiConnected(unsigned long timeoutMs = 5000) {
  if (WiFi.status() == WL_CONNECTED) {
    wifi_ok = true;
    // start HTTP server if not already started
    if (!httpServerStarted) {
      server.begin();
      httpServerStarted = true;
      Serial.println("HTTP server started (reconnect)");
    }
    return true;
  }
  Serial.println("WiFi disconnected, attempting reconnect...");
  WiFi.disconnect();
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - start) < timeoutMs) {
    delay(200);
  }
  wifi_ok = (WiFi.status() == WL_CONNECTED);
  if (wifi_ok) Serial.println("WiFi reconnected");
  else Serial.println("WiFi reconnect failed");
  return wifi_ok;
}

void setup() {
  Serial.begin(115200);
  // Initialize SD card (SPI)
  if (!SD.begin(SD_CS_PIN)) {
    Serial.println("SD mount failed");
  } else {
    Serial.println("SD mounted");
  }

  // Config portal: load MQTT config from SD card if present
  StaticJsonDocument<512> doc;
  loadConfig();
  Serial.println("Runtime MQTT config: ");
  Serial.println(runtimeConfig.mqtt_server);

  // forward declarations
  void handleLogs();

  // Start WiFiManager config portal if Wi-Fi not available
  WiFiManager wm;
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Starting config portal (AP mode) - connect to configure WiFi/MQTT");
    // add custom parameters for MQTT
  WiFiManagerParameter custom_mqtt_server("server", "mqtt server", runtimeConfig.mqtt_server, 64);
  char portbuf[8];
  snprintf(portbuf, sizeof(portbuf), "%d", runtimeConfig.mqtt_port);
  WiFiManagerParameter custom_mqtt_port("port", "mqtt port", portbuf, 6);
  WiFiManagerParameter custom_mqtt_topic("topic", "mqtt topic", runtimeConfig.mqtt_topic, 64);
    wm.addParameter(&custom_mqtt_server);
    wm.addParameter(&custom_mqtt_port);
    wm.addParameter(&custom_mqtt_topic);
    if (!wm.startConfigPortal("UHI_Config")) {
      Serial.println("Failed to enter config portal");
    } else {
      Serial.println("Config portal exited");
      // save parameters to SD
      saveConfig(custom_mqtt_server.getValue(), atoi(custom_mqtt_port.getValue()), custom_mqtt_topic.getValue());
      Serial.println("Saved MQTT config to SD");
    }
  }
  GPSSerial.begin(GPS_BAUD, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
  PMSerial.begin(PM_BAUD, SERIAL_8N1, PM_RX_PIN, PM_TX_PIN);
  dht.begin();

  // Initialize PM2.5 sensor
  Serial.println("Initializing PM2.5 sensor...");
  if (!pm25.begin_UART(&PMSerial)) {
    Serial.println("Failed to initialize PM2.5 sensor");
  } else {
    Serial.println("PM2.5 sensor initialized");
  }

  setupWiFi();
  mqttClient.setServer(runtimeConfig.mqtt_server, runtimeConfig.mqtt_port);

  // Register HTTP route for logs and start server if on WiFi
  server.on("/logs", handleLogs);
  if (WiFi.status() == WL_CONNECTED) {
    server.begin();
    Serial.println("HTTP server started");
  }

  lastPublish = millis();
}

// Serial download helper: when a single capital 'D' is received over Serial,
// stream the contents of /logs.jsonl to Serial. This allows retrieving the
// SD-stored log file over a USB serial connection without removing the card.
void handleSerialDownload() {
  if (!Serial.available()) return;
  int c = Serial.peek();
  if (c != 'D') return;
  // consume the trigger
  Serial.read();
  Serial.println("===BEGIN_LOGS===");
  if (!SD.exists("/logs.jsonl")) {
    Serial.println("ERROR: /logs.jsonl not found");
    Serial.println("===END_LOGS===");
    return;
  }
  File f = SD.open("/logs.jsonl", FILE_READ);
  if (!f) {
    Serial.println("ERROR: failed to open /logs.jsonl");
    Serial.println("===END_LOGS===");
    return;
  }
  // stream file in chunks to avoid large memory usage
  const size_t bufSize = 256;
  char buf[bufSize];
  while (f.available()) {
    size_t r = f.readBytes(buf, min((size_t)f.available(), bufSize));
    Serial.write((const uint8_t*)buf, r);
    // small delay to allow host to keep up
    delay(2);
  }
  f.close();
  Serial.println();
  Serial.println("===END_LOGS===");
}

// HTTP handler to stream /logs.jsonl from SD card.
// Accessible at http://<device-ip>/logs
void handleLogs() {
  if (!SD.exists("/logs.jsonl")) {
    server.send(404, "text/plain", "ERROR: /logs.jsonl not found\n");
    return;
  }
  File f = SD.open("/logs.jsonl", FILE_READ);
  if (!f) {
    server.send(500, "text/plain", "ERROR: failed to open /logs.jsonl\n");
    return;
  }
  // Suggest download filename to browsers
  server.sendHeader("Content-Disposition", "attachment; filename=logs.jsonl");
  // Stream file (this will handle large files without loading into RAM)
  server.streamFile(f, "text/plain");
  f.close();
}

void loop() {
  // check for serial-download trigger before doing other work
  handleSerialDownload();
  // Handle incoming HTTP clients (if server was started)
  server.handleClient();
  // Feed GPS parser
  while (GPSSerial.available() > 0) {
    char c = GPSSerial.read();
    gps.encode(c);
  }

  if (!mqttClient.connected()) {
    reconnectMQTT();
  }
  mqttClient.loop();

  unsigned long now = millis();
  if (now - lastPublish >= PUBLISH_INTERVAL) {
    lastPublish = now;

    // Ensure connectivity before reading/publishing
    ensureWiFiConnected();
    reconnectMQTT();

    // Read sensors
    float temperature = NAN;
    float humidity = NAN;
    // DHT read with retry
    dht_ok = false;
    for (int i = 0; i < 3; ++i) {
      temperature = dht.readTemperature();
      humidity = dht.readHumidity();
      if (!isnan(temperature) && !isnan(humidity)) {
        dht_ok = true;
        break;
      }
      delay(200);
    }

    // GPS
    double lat = 0.0;
    double lng = 0.0;
    bool hasFix = false;
    if (gps.location.isValid()) {
      lat = gps.location.lat();
      lng = gps.location.lng();
      hasFix = true;
      lastGpsFixMillis = now;
    }
    // compute GPS fix age (seconds) later

    // Build ISO8601 timestamp
    char isoTime[32] = {0};
    struct tm timeinfo;
    time_t nowSecs = 0;
    if (getLocalTime(&timeinfo)) {
      nowSecs = mktime(&timeinfo);
      // format like 2025-09-27T12:34:56Z (UTC)
      // use gmtime_r to always format UTC
      struct tm gmtime_res;
      gmtime_r(&nowSecs, &gmtime_res);
      strftime(isoTime, sizeof(isoTime), "%Y-%m-%dT%H:%M:%SZ", &gmtime_res);
    } else {
      // fallback to millis-based timestamp (seconds since boot)
      snprintf(isoTime, sizeof(isoTime), "uptime-%lu", (unsigned long)(now / 1000));
    }

  // Build JSON payload
    // Read PM2.5 with basic error handling/reinit
    float pm2_5 = NAN;
    PM25_AQI_Data data;
    if (pm25.read(&data)) {
      pm2_5 = data.pm25_env;
      pm_ok = true;
      pm_fail_count = 0;
    } else {
      pm_fail_count++;
      pm_ok = false;
      Serial.print("PM read failed (count="); Serial.print(pm_fail_count); Serial.println(")");
      if (pm_fail_count >= PM_FAIL_THRESHOLD) {
        Serial.println("Attempting PM sensor re-init...");
        if (pm25.begin_UART(&PMSerial)) {
          Serial.println("PM sensor re-init succeeded");
          pm_fail_count = 0;
          pm_ok = true;
        } else {
          Serial.println("PM sensor re-init failed");
        }
      }
    }

    // compute gps fix age in seconds
    long gps_fix_age_s = -1;
    if (lastGpsFixMillis != 0) gps_fix_age_s = (long)((now - lastGpsFixMillis) / 1000);

  // Build JSON with health fields
  char payload[640];
    String devId = getDeviceId();
    int len = snprintf(payload, sizeof(payload), "{\"device\":\"esp32\",\"device_id\":\"%s\",\"timestamp\":\"%s\",\"gps_fix\":%s,\"gps_fix_age_s\":%ld,\"lat\":%.6f,\"lng\":%.6f,\"temp_c\":%.2f,\"humidity_pct\":%.2f,\"pm2_5\":%.2f,\"wifi_ok\":%s,\"mqtt_ok\":%s,\"dht_ok\":%s,\"pm_ok\":%s}",
      devId.c_str(), isoTime,
      hasFix ? "true" : "false",
      gps_fix_age_s,
      hasFix ? lat : 0.0,
      hasFix ? lng : 0.0,
      isnan(temperature) ? 0.0 : temperature,
      isnan(humidity) ? 0.0 : humidity,
      isnan(pm2_5) ? 0.0 : pm2_5,
      wifi_ok ? "true" : "false",
      mqtt_ok ? "true" : "false",
      dht_ok ? "true" : "false",
      pm_ok ? "true" : "false"
    );

    Serial.print("Publishing: ");
    Serial.println(payload);

    // Append payload to local log on SD card for offline troubleshooting
    File lf = SD.open("/logs.jsonl", FILE_APPEND);
    if (lf) {
      lf.println(payload);
      lf.close();
    } else {
      Serial.println("Failed to open /logs.jsonl on SD");
    }

    bool published = false;
    if (mqttClient.connected()) {
      if (mqttClient.publish(runtimeConfig.mqtt_topic, payload)) {
        published = true;
      } else {
        Serial.println("Publish failed");
      }
    } else {
      Serial.println("MQTT not connected, skipping publish");
    }

    // If deep sleep is enabled, enter sleep to save battery
    if (ENABLE_DEEP_SLEEP) {
      unsigned long sleepSec = DEEP_SLEEP_SECONDS;
      if (sleepSec == 0) sleepSec = 60; // sanity default
      Serial.print("Entering deep sleep for "); Serial.print(sleepSec); Serial.println(" seconds...");
      // give serial time to flush
      Serial.flush();
      esp_sleep_enable_timer_wakeup((uint64_t)sleepSec * 1000000ULL);
      esp_deep_sleep_start();
      // device will reset here on wake
    }
  }
}
