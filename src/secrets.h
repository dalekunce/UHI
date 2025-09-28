// secrets.h - store your WiFi and MQTT credentials here
// IMPORTANT: This file is included in .gitignore by default to avoid committing secrets.

#ifndef _SECRETS_H_
#define _SECRETS_H_

// WiFi
// Example: const char* WIFI_SSID = "MySSID";
extern const char* WIFI_SSID;
extern const char* WIFI_PASS;

// MQTT
extern const char* MQTT_SERVER;
extern int MQTT_PORT;
extern const char* MQTT_TOPIC;

// Deep sleep settings
extern const bool ENABLE_DEEP_SLEEP; // true to enable sleeping between publishes
extern const unsigned long DEEP_SLEEP_SECONDS; // sleep duration in seconds

#endif // _SECRETS_H_
