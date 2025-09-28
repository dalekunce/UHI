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
	Each telemetry JSON payload now includes a stable `device_id` field. By default this is the device's burned-in Wi‑Fi MAC (efuse MAC) formatted as a colon-separated address (for example: `AA:BB:CC:DD:EE:FF`). This is useful for correlating MQTT messages, logs on the SD card, and inventory records.

	Privacy / alternatives
	----------------------
	- If you prefer not to expose the raw MAC, you can change the firmware to generate and store a random UUID on first boot (saved to SD or NVS) and publish that instead.
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

	CI / GitHub Actions
	-------------------
	A GitHub Actions workflow is included at `.github/workflows/platformio.yml` that builds the firmware on push and pull requests using PlatformIO. This helps catch missing dependencies or build regressions early.

	Repository hygiene and notes
	---------------------------
	- Real secret files (`src/secrets.cpp`) are Git-ignored. Never commit private keys or passwords.
	- Example secret files (`src/secrets.example.*`) are safe to commit and are included to help new contributors onboard quickly.
	- `.gitignore` excludes PlatformIO build artifacts, editor files, and local secrets.

	Integration notes
	-----------------
	This project can work with either the SPI `SD` library (external microSD module) or the `SD_MMC` interface (native SD slot on some ESP32 boards). Update `src/main.cpp` if you need to change pins or the SD interface.

	Deep sleep & battery tips
	-------------------------
	Enable deep sleep in `src/secrets.cpp` by setting `ENABLE_DEEP_SLEEP = true` and choose an appropriate `DEEP_SLEEP_SECONDS`. Remember that deep sleep cycles will reinitialize Wi‑Fi and sensors on each wake, which affects power consumption.


	Next steps and contribution ideas
	---------------------------------
	- Add a small script to copy example secrets into active secrets files with interactive prompts
	- Add CI checks to scan for accidentally committed secrets

	License and attribution
	-----------------------
	See `LICENSE` (if present) and individual source headers for upstream library licenses.

