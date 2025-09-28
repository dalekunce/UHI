ESP32 UHI Sensor Publisher

Overview
--------
This project targets an ESP32 Dev board, publishes environment and location telemetry via MQTT at regular intervals, and saves each telemetry record locally to an SD card for offline troubleshooting and archival. It currently collects:

- GPS location (TinyGPSPlus)
- Temperature and humidity (DHT22 or DHT11 via Adafruit DHT library)
- Particulate matter PM2.5 (PMSxxxx series via Adafruit PM25AQI)

Every publish includes an ISO 8601 UTC timestamp and basic health flags so you can monitor device/sensor status remotely.

Features
--------
- Periodic telemetry publish via MQTT (configurable interval).
- Local append-only logging to an SD card (`/logs.jsonl`) for offline troubleshooting and archival.
- Multiple access methods for logs:
	- Remove the SD card and read `logs.jsonl` on a computer.
	- Serial-download: send a single capital `D` over USB serial to stream logs between markers (`===BEGIN_LOGS===` / `===END_LOGS===`). Host helper: `scripts/download_sd.py`.
	- HTTP download: GET `http://<device-ip>/logs` to stream/download `logs.jsonl` when the device is connected to Wi‑Fi.
- Captive config portal (WiFiManager) that lets you set Wi‑Fi and MQTT server/port/topic; saved to `/config.json` on the SD card.
- Sensors supported: GPS (TinyGPSPlus), DHT22/11 (temperature & humidity), and PMSxxxx PM2.5 (Adafruit PM25AQI).
- Health flags in every JSON payload (wifi_ok, mqtt_ok, dht_ok, pm_ok) and GPS fix/age fields.
- Deep sleep support (configurable via `src/secrets.cpp`) for battery-powered deployments.
- Desktop helper scripts:
	- `scripts/download_sd.py` — serial download helper.
	- `scripts/hw_smoke_test.py` — automated hardware smoke test that performs a serial download and validates the logs file.


JSON payload example
--------------------
{
	"device":"esp32",
	"timestamp":"2025-09-27T12:34:56Z",
	"gps_fix":true,
	"gps_fix_age_s":12,
	"lat":37.123456,
	"lng":-122.123456,
	"temp_c":22.50,
	"humidity_pct":55.20,
	"pm2_5":12.30,
	"wifi_ok":true,
	"mqtt_ok":true,
	"dht_ok":true,
	"pm_ok":true
}

Hardware
--------
You will need:

- An ESP32 Dev board (e.g., ESP32 DevKitC)
- TTL GPS module (e.g., NEO-6M)
- DHT22 (or DHT11) sensor
- PMSxxxx PM2.5 sensor module (PMS5003/PMS7003) or similar TTL PM sensor
- Wires, breadboard, optional level shifters if your modules use 5V logic

Wiring
-------
See `WIRING.md` for a simple ASCII diagram and pin mapping. Default pins used in `src/main.cpp`:

If your board labels differ, update the pin defines in `src/main.cpp`.

Configuration (secrets)
-----------------------
Credentials and runtime settings live in `src/secrets.cpp` (values declared in `src/secrets.h`). This file is ignored by Git to avoid leaking secrets.

Required fields in `src/secrets.cpp`:

- WIFI_SSID, WIFI_PASS
- MQTT_SERVER, MQTT_PORT, MQTT_TOPIC
- ENABLE_DEEP_SLEEP (bool) — set to true for battery mode
- DEEP_SLEEP_SECONDS (unsigned long) — sleep duration when enabled

Build & Flash
-------------
Recommended: PlatformIO (VS Code extension)

PlatformIO CLI commands (run in project root):

```bash
pio run           # builds the firmware
pio run -t upload # builds and uploads to connected board
pio device monitor -b 115200
```

Alternatively, open `src/main.cpp` in Arduino IDE, install required libraries (TinyGPSPlus, DHT sensor library, PubSubClient, Adafruit PM25AQI), select the correct ESP32 board and upload.

Testing with MQTTX (or other MQTT client)
-----------------------------------------
1. Fill `src/secrets.cpp` with your Wi‑Fi and MQTT broker details.
2. Flash the device and open the serial monitor at 115200 bps.
3. In MQTTX, subscribe to the configured topic (default: `esp32/sensors/uhi`).
4. Expect a JSON message every publish interval (default 10s). Health flags in the JSON indicate if sensors read successfully.


Config portal (captive Wi‑Fi)
--------------------------------
- If the device cannot connect to Wi‑Fi on boot it will start a captive portal AP called `UHI_Config`.
- Connect a phone or laptop to that network and a web page will appear where you can enter Wi‑Fi credentials and MQTT server/port/topic.
- Those MQTT settings are saved to the SD card as `/config.json` and loaded automatically on subsequent boots.

Local logging (SD card)
-----------------------
- All published JSON payloads are appended to `logs.jsonl` on the SD card (one JSON payload per line). The device broadcasts the same JSON over MQTT each publish interval, so logs are both transmitted and stored locally.
- Access methods:
	- Remove the SD card and read it on a computer (FAT32 friendly).
	- If using an SD slot wired to the board's SDMMC interface, use the host reader or an adapter to access files.

<!-- Local log parsing helper removed; use your own tools or the SD card/serial/http download methods described above. -->

Serial download (retrieve logs without removing SD)
-----------------------------------------------
If you prefer to retrieve `logs.jsonl` over the USB serial connection, the firmware supports a simple serial-download trigger. Send a single capital 'D' character over the serial port and the device will stream the contents of `/logs.jsonl` wrapped between markers:

	===BEGIN_LOGS===
	...file contents...
	===END_LOGS===

There's also a host helper script included at `scripts/download_sd.py` which sends the trigger and saves the streamed logs to a local file. Example (macOS):

```bash
python3 scripts/download_sd.py /dev/tty.SLAB_USBtoUART logs.jsonl
```

The script uses `pyserial`. If it's not installed you'll see a hint to run `pip3 install pyserial`.

Hardware smoke test
-------------------
For a quick end-to-end hardware verification (SD logging + serial download), use the included smoke test helper which automates the serial download and validates the file:

```bash
# run the automated hardware smoke test (replace serial port)
python3 scripts/hw_smoke_test.py /dev/tty.SLAB_USBtoUART
```

See `TESTING.md` for a checklist and troubleshooting tips.


Integration notes
-----------------
- This project can use either the SPI `SD` library or the `SD_MMC` (native) interface on the ESP32. Choose based on your hardware:
	- SD (SPI): common when using an external microSD module. Typical example pins (change in `src/main.cpp` as needed): CS=5, MOSI=23, MISO=19, SCK=18.
	- SD_MMC: use if your board has an SD card slot wired to the SDMMC lines (often only needs 1 or 2 pins exposed depending on board).
- Example pseudocode to append a log entry:

	```cpp
	if (SD.begin(CS_PIN)) {
		File f = SD.open("/logs.jsonl", FILE_APPEND);
		if (f) {
			f.println(payloadJsonString);
			f.close();
		}
	}
	```

- Keep power and card wear in mind: repeatedly writing small lines will cause more frequent SD writes than bulk appends. If running on battery, consider buffering several payloads in RAM and flushing to SD infrequently.
- The device still publishes each payload to MQTT (LWT and health flags are supported) — SD is an additional local store, not a replacement for MQTT.

Deep sleep & battery tips
-------------------------
- Enable deep sleep in `src/secrets.cpp` by setting `ENABLE_DEEP_SLEEP = true` and choose an appropriate `DEEP_SLEEP_SECONDS`.
- Deep sleep restarts the device on wake. Wi‑Fi and sensor initialization occur each cycle — this costs power.

If you prefer to retrieve logs over serial instead of removing the SD card, the firmware includes a serial helper that streams the contents of `/logs.jsonl` over Serial when triggered (send a single capital 'D'). A host helper script `scripts/download_sd.py` is provided to automate this process.
