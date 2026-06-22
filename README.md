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


Firmware Variants
-----------------
### Main Variant (MQTT + SD logging)
The default `src/` variant publishes telemetry via MQTT and logs JSON records to `/logs.jsonl` on the SD card.

**Build & flash:**
```bash
pio run -e esp32dev              # build
pio run -e esp32dev -t upload    # upload
```

### SD-Only Variant (GLOBE airTemps CSV)
The `src-sd-only/` variant removes all WiFi/MQTT dependencies and writes records directly to `/airtemps_globe.csv` in the GLOBE/NASA air temperature data schema format. This is useful for offline data collection or integration with the GLOBE Observer platform.

**Output format:** CSV with 18 columns (15 GLOBE standard + 3 sensor extensions):
```
organization_id,org_name,site_id,site_name,latitude,longitude,elevation,measured_on,userid,measured_at,solar_measured_at,current_temp,humidity_pct,pm2_5,device_id,comments,globe_teams,month
,,189484,Dublin School,42.90861,-72.06333,488.0,2025-09-01,8294323,2025-09-01T00:01:00,,,18.5,55.20,12.30,550e8400-e29b-41d4-a716-446655440000,dht_ok=true;pm_ok=true,network_id,Sep
```

Extra sensor fields—`humidity_pct` (%), `pm2_5` (μg/m³), and `device_id` (UUID)—are written in their own columns. The `comments` field now contains only status flags.

**Configuration:** Edit `src-sd-only/secrets.cpp` and populate GLOBE metadata fields before building:
- `GLOBE_ORGANIZATION_ID` — Your organization's GLOBE ID (numeric string)
- `GLOBE_ORG_NAME` — Organization name
- `GLOBE_SITE_ID` — Your observation site ID (numeric string)
- `GLOBE_SITE_NAME` — Site name
- `GLOBE_ELEVATION_M` — Elevation above sea level (meters, as string)
- `GLOBE_USER_ID` — Your GLOBE user ID (numeric string)
- `GLOBE_TEAMS` — Comma-separated team IDs or names

**Build & flash:**
```bash
pio run -e esp32dev-sd-only              # build
pio run -e esp32dev-sd-only -t upload    # upload
```

**Data retrieval:** Same as main variant—send `D` over serial or remove SD card to access `/airtemps_globe.csv`.

**Reference:** See [GLOBE Air Temperature Data Protocol](https://assets.globe.gov/protocol-data/airTemps/airTemps2025/airTemps2025Sep.csv) for official schema details.


JSON payload example
--------------------
{
	"device":"esp32",
	# ESP32 UHI Sensor Publisher

	Overview
	--------
	This firmware targets an ESP32 Dev board and publishes periodic environment and location telemetry via MQTT while also saving every telemetry record to an SD card for offline troubleshooting and archival.

	Sensors and data collected
	- GPS location (TinyGPSPlus)
	- Temperature and humidity (DHT22 or DHT11 via Adafruit DHT library)
	- Particulate matter PM2.5 (PMSxxxx series via Adafruit PM25AQI)

	Each published payload includes an ISO 8601 UTC timestamp and basic health flags so you can monitor device/sensor status remotely.

	Key features
	------------
	- Periodic telemetry publish via MQTT (configurable interval)
	- Local append-only logging to an SD card (`/logs.jsonl`) for offline troubleshooting and archival
	- Multiple log retrieval methods: remove SD card, serial-download, or HTTP download when on Wi‑Fi
	- Captive config portal (WiFiManager) for on-device Wi‑Fi and MQTT configuration; saved to `/config.json` on the SD card
	- Health flags (wifi_ok, mqtt_ok, dht_ok, pm_ok) and GPS fix/age fields included in every payload
	- Deep sleep support for battery-powered deployments (configurable)
	- Desktop helper scripts: `scripts/download_sd.py` (serial log download) and `scripts/hw_smoke_test.py` (hardware smoke test)

	Device identifier (device_id)
	-----------------------------
	Each telemetry JSON payload includes a stable `device_id` field. By default the firmware generates a UUIDv4 on first boot and saves it to the SD card in `/config.json` under the `device_id` key. The firmware reads that file on subsequent boots and publishes the same UUID with every telemetry record. This makes the identifier stable across reboots and avoids exposing the raw hardware MAC.

	Privacy / alternatives
	----------------------
	- If you prefer not to expose the raw MAC, you can change the firmware to generate and store a random UUID on first boot (saved to SD or NVS) and publish that instead.
	- This firmware uses the SD-based UUID method by default (see `/config.json` `device_id`). If SD is unavailable at boot the firmware falls back to publishing the device's MAC address.
	- Another option is to publish a hash of the MAC (e.g., SHA1) to avoid exposing the full hardware address while keeping a stable identifier. If you want either of these behaviors I can add the implementation and an option in `src/secrets.example.cpp`.

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
	Required hardware includes:

	- An ESP32 Dev board (e.g., ESP32 DevKitC)
	- TTL GPS module (e.g., NEO-6M)
	- DHT22 or DHT11 sensor
	- PMSxxxx PM2.5 sensor module (PMS5003/PMS7003) or equivalent
	- Wires, breadboard, and optional level shifters

	See `WIRING.md` for a simple ASCII diagram and pin mapping. If your board's labels differ, update the pin defines in `src/main.cpp`.

	Configuration (secrets)
	-----------------------
	Runtime credentials and settings live in `src/secrets.cpp` and are declared in `src/secrets.h`. Real secret files are intentionally Git-ignored.

	To make it easy to get started without exposing secrets, example placeholder files are included:

	- `src/secrets.example.h`
	- `src/secrets.example.cpp`

	Copy those into real files and edit values before building:

	```bash
	cp src/secrets.example.h src/secrets.h
	cp src/secrets.example.cpp src/secrets.cpp
	# then edit src/secrets.cpp and fill in WIFI, MQTT, and sleep settings
	```

	Required configuration values (examples defined in the header/implementation):
	- WIFI_SSID, WIFI_PASS
	- MQTT_SERVER, MQTT_PORT, MQTT_TOPIC
	- ENABLE_DEEP_SLEEP (bool)
	- DEEP_SLEEP_SECONDS (unsigned long)

	Build & flash
	-------------
	Recommended: PlatformIO (VS Code extension)

	PlatformIO CLI (run in project root):

	```bash
	pio run           # builds the firmware
	pio run -t upload # builds and uploads to connected board
	pio device monitor -b 115200
	```

	Serial download (retrieve logs without removing SD)
	-------------------------------------------------
	Send a single capital `D` character over the serial port and the device will stream `/logs.jsonl` between markers:

	  ===BEGIN_LOGS===
	  ...file contents...
	  ===END_LOGS===

	Use the host helper script to automate this interaction:

	```bash
	python3 scripts/download_sd.py /dev/tty.SLAB_USBtoUART logs.jsonl
	```

	The script uses `pyserial` (pip package `pyserial`).

	Hardware smoke test
	-------------------
	Run the automated hardware smoke test to verify SD logging and serial download (replace the serial device):

	```bash
	python3 scripts/hw_smoke_test.py /dev/tty.SLAB_USBtoUART
	```

	Integration notes
	-----------------
	This project can work with either the SPI `SD` library (external microSD module) or the `SD_MMC` interface (native SD slot on some ESP32 boards). Update `src/main.cpp` if you need to change pins or the SD interface.

	Deep sleep & battery tips
	-------------------------
	Enable deep sleep in `src/secrets.cpp` by setting `ENABLE_DEEP_SLEEP = true` and choose an appropriate `DEEP_SLEEP_SECONDS`. Remember that deep sleep cycles will reinitialize Wi‑Fi and sensors on each wake, which affects power consumption.