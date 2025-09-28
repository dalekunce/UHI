Hardware smoke test checklist
=============================

This checklist helps you verify the core hardware+firmware features: SD logging, MQTT publishing, config portal, and serial-download.

Prerequisites
- A flashed ESP32 device with the firmware from this repo.
- A FAT32-formatted microSD card inserted.
- USB serial cable to the ESP32 and a host machine with Python 3.
- (Optional) MQTT broker reachable from the device for MQTT verification.

Quick steps
1. Power & boot
   - Insert SD card and power the device.
   - Open serial monitor at 115200 baud.
   - Confirm the device prints "SD mounted" and runtime MQTT config.

2. MQTT smoke
   - If you have an MQTT broker configured in `src/secrets.cpp` or via the config portal, subscribe to the device topic (default `esp32/sensors/uhi`) from an MQTT client (e.g., MQTTX) and confirm JSON messages appear at the publish interval.

3. Local logging to SD
   - After a few publish cycles, unmount the card (or use the serial-download helper) and check that `/logs.jsonl` has been created and contains JSON objects, one per line.

4. Serial-download test (no card removal)
   - From your host, run the provided helper to fetch `logs.jsonl` over serial (replace the device path):

```bash
python3 scripts/download_sd.py /dev/tty.SLAB_USBtoUART logs.jsonl
```

   - Confirm the file `logs.jsonl` is saved and contains lines of JSON.

5. Run the smoke test helper (automated)
   - The repo includes `scripts/hw_smoke_test.py` which:
     - Runs `download_sd.py` against the provided serial port
     - Validates the `logs.jsonl` file (counts lines, ensures valid JSON, reports first/last timestamps)

   - Example:

```bash
python3 scripts/hw_smoke_test.py /dev/tty.SLAB_USBtoUART
```

Troubleshooting
- If SD mount fails: check CS pin wiring, card orientation, and 3.3V power.
- If MQTT doesn't connect: verify Wi‑Fi credentials and broker reachability; try the config portal (AP mode) to set MQTT settings.
- If serial-download times out: ensure the device is connected and serial monitor (or other program) isn't holding the port open.

Notes
- The hardware smoke test script requires `pyserial` installed on the host (pip3 install pyserial).
- The serial-download script follows the simple trigger protocol: send 'D' then capture data between `===BEGIN_LOGS===` and `===END_LOGS===` markers.

"""
