#include <Arduino.h>
#include <TinyGPSPlus.h>
#include <HardwareSerial.h>
#include <DHT.h>
#include <Adafruit_PM25AQI.h>
#include <esp_sleep.h>
#include <SPI.h>
#include <SD.h>
#include <ArduinoJson.h>

// ======= CONFIG =======
#include "secrets.h"

#define SD_CS_PIN 5   // default CS pin for SPI SD modules; change if needed

// DHT settings
#define DHTPIN 4       // GPIO where DHT is connected
#define DHTTYPE DHT22  // DHT11 or DHT22

// GPS UART
#define GPS_RX_PIN 16  // GPS TX -> ESP32 RX
#define GPS_TX_PIN 17  // GPS RX -> ESP32 TX (not used if GPS is TX-only)
#define GPS_BAUD 9600

// PM2.5 UART
#define PM_RX_PIN 26   // PM sensor TX -> ESP32 RX2
#define PM_TX_PIN 27   // PM sensor RX -> ESP32 TX2 (if required)
#define PM_BAUD 9600

// Record interval in milliseconds
const unsigned long RECORD_INTERVAL = 10000UL;
const char* GLOBE_AIRTEMPS_FILE = "/airtemps_globe.csv";
const char* GLOBE_AIRTEMPS_HEADER = "organization_id,org_name,site_id,site_name,latitude,longitude,elevation,measured_on,userid,measured_at,solar_measured_at,current_temp,humidity_pct,pm2_5,device_id,comments,globe_teams,month";

// =============================================

TinyGPSPlus gps;
HardwareSerial GPSSerial(1);
DHT dht(DHTPIN, DHTTYPE);
HardwareSerial PMSerial(2);
Adafruit_PM25AQI pm25;

bool pm_ok = false;
bool dht_ok = false;
unsigned long lastGpsFixMillis = 0;
int pm_fail_count = 0;
const int PM_FAIL_THRESHOLD = 3;

unsigned long lastRecord = 0;

void writeCsvText(File& f, const char* s) {
  f.print('"');
  if (s) {
    while (*s) {
      if (*s == '"') f.print('"');
      f.print(*s);
      ++s;
    }
  }
  f.print('"');
}

void writeCsvFloatOrBlank(File& f, float v, int decimals) {
  if (isnan(v)) return;
  f.print(v, decimals);
}

void buildMeasuredAt(char* out, size_t outLen) {
  if (gps.date.isValid() && gps.time.isValid()) {
    snprintf(out, outLen, "%04d-%02d-%02dT%02d:%02d:%02d",
             gps.date.year(), gps.date.month(), gps.date.day(),
             gps.time.hour(), gps.time.minute(), gps.time.second());
    return;
  }
  out[0] = '\0';
}

void buildMeasuredOnAndMonth(const char* measuredAt, char* measuredOnOut, size_t measuredOnLen,
                             char* monthOut, size_t monthLen) {
  measuredOnOut[0] = '\0';
  monthOut[0] = '\0';
  if (!measuredAt || strlen(measuredAt) < 10) return;

  strlcpy(measuredOnOut, measuredAt, measuredOnLen);
  measuredOnOut[10] = '\0';

  int monthNum = (measuredAt[5] - '0') * 10 + (measuredAt[6] - '0');
  const char* months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
  if (monthNum >= 1 && monthNum <= 12) {
    strlcpy(monthOut, months[monthNum - 1], monthLen);
  }
}

void ensureGlobeCsvHeader() {
  bool needsHeader = !SD.exists(GLOBE_AIRTEMPS_FILE);
  if (!needsHeader) {
    File rf = SD.open(GLOBE_AIRTEMPS_FILE, FILE_READ);
    if (!rf || rf.size() == 0) needsHeader = true;
    if (rf) rf.close();
  }
  if (!needsHeader) return;

  File wf = SD.open(GLOBE_AIRTEMPS_FILE, FILE_WRITE);
  if (!wf) {
    Serial.println("Failed to create GLOBE CSV file");
    return;
  }
  wf.println(GLOBE_AIRTEMPS_HEADER);
  wf.close();
}

// UUID helpers: generate UUIDv4 and persist in /config.json on SD
static void generateUuidV4(char* out /* 37 bytes */) {
  uint8_t b[16];
  for (int i = 0; i < 4; ++i) {
    uint32_t r = esp_random();
    memcpy(&b[i * 4], &r, 4);
  }
  b[6] = (b[6] & 0x0F) | 0x40;  // version 4
  b[8] = (b[8] & 0x3F) | 0x80;  // RFC4122 variant
  snprintf(out, 37,
           "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
           b[0], b[1], b[2], b[3], b[4], b[5], b[6], b[7], b[8], b[9],
           b[10], b[11], b[12], b[13], b[14], b[15]);
}

String getDeviceId() {
  const char* path = "/config.json";
  if (!SD.begin(SD_CS_PIN)) {
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    char buf[18];
    snprintf(buf, sizeof(buf), "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return String(buf);
  }

  StaticJsonDocument<256> doc;
  if (SD.exists(path)) {
    File f = SD.open(path, FILE_READ);
    if (f) {
      DeserializationError err = deserializeJson(doc, f);
      f.close();
      if (err) doc.clear();
    }
  }

  if (doc.containsKey("device_id")) {
    String s = String(doc["device_id"].as<const char*>());
    s.trim();
    if (s.length() > 0) return s;
  }

  char uuid[37] = {0};
  generateUuidV4(uuid);
  doc["device_id"] = uuid;

  File fw = SD.open(path, FILE_WRITE);
  if (fw) {
    serializeJson(doc, fw);
    fw.close();
  }
  return String(uuid);
}

void setup() {
  Serial.begin(115200);

  if (!SD.begin(SD_CS_PIN)) {
    Serial.println("SD mount failed");
  } else {
    Serial.println("SD mounted");
    ensureGlobeCsvHeader();
  }

  GPSSerial.begin(GPS_BAUD, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
  PMSerial.begin(PM_BAUD, SERIAL_8N1, PM_RX_PIN, PM_TX_PIN);
  dht.begin();

  Serial.println("Initializing PM2.5 sensor...");
  if (!pm25.begin_UART(&PMSerial)) {
    Serial.println("Failed to initialize PM2.5 sensor");
  } else {
    Serial.println("PM2.5 sensor initialized");
  }

  lastRecord = millis();
}

// Serial download helper: send a single capital 'D' over USB serial to
// stream the GLOBE-compatible CSV file between markers.
void handleSerialDownload() {
  if (!Serial.available()) return;
  int c = Serial.peek();
  if (c != 'D') return;
  Serial.read();
  Serial.println("===BEGIN_LOGS===");
  if (!SD.exists(GLOBE_AIRTEMPS_FILE)) {
    Serial.println("ERROR: airtemps_globe.csv not found");
    Serial.println("===END_LOGS===");
    return;
  }
  File f = SD.open(GLOBE_AIRTEMPS_FILE, FILE_READ);
  if (!f) {
    Serial.println("ERROR: failed to open airtemps_globe.csv");
    Serial.println("===END_LOGS===");
    return;
  }
  const size_t bufSize = 256;
  char buf[bufSize];
  while (f.available()) {
    size_t r = f.readBytes(buf, min((size_t)f.available(), bufSize));
    Serial.write((const uint8_t*)buf, r);
    delay(2);
  }
  f.close();
  Serial.println();
  Serial.println("===END_LOGS===");
}

void loop() {
  handleSerialDownload();

  // Feed GPS parser
  while (GPSSerial.available() > 0) {
    gps.encode(GPSSerial.read());
  }

  unsigned long now = millis();
  if (now - lastRecord >= RECORD_INTERVAL) {
    lastRecord = now;

    // Read DHT with retry
    float temperature = NAN;
    float humidity = NAN;
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

    // GPS location
    double lat = 0.0;
    double lng = 0.0;
    bool hasFix = false;
    if (gps.location.isValid()) {
      lat = gps.location.lat();
      lng = gps.location.lng();
      hasFix = true;
      lastGpsFixMillis = now;
    }
    long gps_fix_age_s = (lastGpsFixMillis != 0)
                             ? (long)((now - lastGpsFixMillis) / 1000)
                             : -1;

    // Timestamp and date fields used by GLOBE schema
    char measuredAt[24] = {0};
    char measuredOn[16] = {0};
    char monthName[8] = {0};
    buildMeasuredAt(measuredAt, sizeof(measuredAt));
    buildMeasuredOnAndMonth(measuredAt, measuredOn, sizeof(measuredOn), monthName, sizeof(monthName));

    // Read PM2.5
    float pm2_5 = NAN;
    PM25_AQI_Data data;
    if (pm25.read(&data)) {
      pm2_5 = data.pm25_env;
      pm_ok = true;
      pm_fail_count = 0;
    } else {
      pm_fail_count++;
      pm_ok = false;
      Serial.print("PM read failed (count=");
      Serial.print(pm_fail_count);
      Serial.println(")");
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

    // Build status/health info for comments field
    char comments[96] = {0};
    snprintf(comments, sizeof(comments),
             "dht_ok=%s;pm_ok=%s;gps_fix_age_s=%ld",
             dht_ok ? "true" : "false",
             pm_ok ? "true" : "false",
             gps_fix_age_s);

    ensureGlobeCsvHeader();
    File lf = SD.open(GLOBE_AIRTEMPS_FILE, FILE_APPEND);
    if (lf) {
      writeCsvText(lf, GLOBE_ORGANIZATION_ID); lf.print(',');
      writeCsvText(lf, GLOBE_ORG_NAME); lf.print(',');
      writeCsvText(lf, GLOBE_SITE_ID); lf.print(',');
      writeCsvText(lf, GLOBE_SITE_NAME); lf.print(',');
      if (hasFix) lf.print(lat, 5);
      lf.print(',');
      if (hasFix) lf.print(lng, 5);
      lf.print(',');
      writeCsvText(lf, GLOBE_ELEVATION_M); lf.print(',');
      writeCsvText(lf, measuredOn); lf.print(',');
      writeCsvText(lf, GLOBE_USER_ID); lf.print(',');
      writeCsvText(lf, measuredAt); lf.print(',');
      writeCsvText(lf, ""); lf.print(',');
      writeCsvFloatOrBlank(lf, temperature, 1); lf.print(',');
      writeCsvFloatOrBlank(lf, humidity, 2); lf.print(',');
      writeCsvFloatOrBlank(lf, pm2_5, 2); lf.print(',');
      writeCsvText(lf, getDeviceId().c_str()); lf.print(',');
      writeCsvText(lf, comments); lf.print(',');
      writeCsvText(lf, GLOBE_TEAMS); lf.print(',');
      writeCsvText(lf, monthName);
      lf.println();
      lf.close();
      Serial.println("Appended GLOBE CSV row");
    } else {
      Serial.println("Failed to open airtemps_globe.csv on SD");
    }

    // Deep sleep if enabled
    if (ENABLE_DEEP_SLEEP) {
      unsigned long sleepSec = DEEP_SLEEP_SECONDS;
      if (sleepSec == 0) sleepSec = 60;
      Serial.print("Entering deep sleep for ");
      Serial.print(sleepSec);
      Serial.println(" seconds...");
      Serial.flush();
      esp_sleep_enable_timer_wakeup((uint64_t)sleepSec * 1000000ULL);
      esp_deep_sleep_start();
    }
  }
}
