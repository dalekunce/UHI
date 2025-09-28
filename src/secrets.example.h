// secrets.example.h - example secrets file
// Copy this to src/secrets.h (or use it as a reference) and fill in your
// network and broker details. Be sure NOT to commit your real secrets.

#ifndef _SECRETS_EXAMPLE_H_
#define _SECRETS_EXAMPLE_H_

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

#endif // _SECRETS_EXAMPLE_H_
