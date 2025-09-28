Wiring suggestions

DHT22:
- VCC -> 3.3V
- GND -> GND
- DATA -> GPIO4 (pull-up 10K)

GPS (typical TTL GPS module like NEO-6M):
- GPS TX -> ESP32 RX (GPIO16)
- GPS RX -> ESP32 TX (GPIO17) [may be unused]
- VCC -> 3.3V or 5V depending on module
- GND -> GND

PM2.5 (e.g., PMS5003 / PMS7003 TTL):
- PM TX -> ESP32 RX2 (GPIO26)
- PM RX -> ESP32 TX2 (GPIO27) [may be unused depending on module]
- VCC -> 5V (or 3.3V if module supports it)
- GND -> GND

# Wiring suggestions

This file shows recommended wiring for an ESP32 DevKit running the UHI firmware.
It covers the DHT22, a TTL GPS module, a PMSxxxx PM2.5 sensor, and a typical SPI SD
card breakout. Pin names vary between ESP32 board variants — use the silkscreen on
your board to confirm.

---

## Summary pin mapping (firmware defaults)
```
DHT22 DATA  -> GPIO4
GPS TX      -> GPIO16 (ESP RX1)
GPS RX      -> GPIO17 (ESP TX1) [optional]
PM TX       -> GPIO26 (ESP RX2)
PM RX       -> GPIO27 (ESP TX2) [optional]
SD (SPI)    -> SCK: GPIO18, MISO: GPIO19, MOSI: GPIO23, CS: GPIO5
```

These pins are defined in `src/main.cpp` as:
```
#define DHTPIN 4
#define GPS_RX_PIN 16
#define GPS_TX_PIN 17
#define PM_RX_PIN 26
#define PM_TX_PIN 27
#define SD_CS_PIN 5
```

---

## DHT22
- VCC -> 3.3V
- GND -> GND
- DATA -> GPIO4 (use a 10k pull-up to VCC)

Notes:
- Use the DHT22 (AM2302) sensor type in `src/main.cpp` (`DHTTYPE DHT22`).

---

## GPS (TTL module, e.g., NEO-6M)
- GPS TX -> GPIO16 (ESP RX1)
- GPS RX -> GPIO17 (ESP TX1) [many modules are TX-only and don't need RX]
- VCC -> 3.3V (or 5V if the module accepts it)
- GND -> GND

Notes:
- If the GPS module uses 5V logic, use a level shifter on the TX line into the ESP RX pin.

---

## PM2.5 (PMS5003 / PMS7003 TTL)
- PM TX -> GPIO26 (ESP RX2)
- PM RX -> GPIO27 (ESP TX2) [optional]
- VCC -> 5V (or 3.3V depending on module)
- GND -> GND

Notes:
- Many PM sensor modules only output TX; only the PM TX -> ESP RX pin is required.

---

## SD card (SPI) module — recommended wiring
- VCC  -> 3.3V (do NOT wire raw SD card to 5V)
- GND  -> GND
- SCK  -> GPIO18
- MISO -> GPIO19
- MOSI -> GPIO23
- CS   -> GPIO5 (matches `#define SD_CS_PIN 5` in `src/main.cpp`)

Notes:
- If your breakout has a regulator and level shifters it may accept 5V, but 3.3V is safest.
- If you want to use different SPI pins or HSPI/VSPI explicitly, update `SPI.begin(...)` and
  `SD.begin(...)` accordingly in the code.

---

## ASCII wiring diagram (ESP32 DevKit example)

```
                    USB
                    |
                ┌──────────────┐
                │  ESP32 DEV   │
                │   (DevKit)   │
          3V3 o─│  [ 3V3 ]     │ o GND
          (VCC) │              │
                │  [ EN  ]     │
                │              │
  DHT DATA o───│> [ GPIO4 ]   │ o GPIO2
                │              │
                │  [ GPIO16 ]  │<── GPS TX (GPS -> ESP RX1)
                │  [ GPIO17 ]  │──> GPS RX (GPS <- ESP TX1)
                │              │
                │  [ GPIO26 ]  │<── PM TX (PM -> ESP RX2)
                │  [ GPIO27 ]  │──> PM RX (PM <- ESP TX2)
                │              │
                │  [ GPIO18 ]  │<── SD SCK
                │  [ GPIO19 ]  │──> SD MISO
                │  [ GPIO23 ]  │<── SD MOSI
                │  [ GPIO5  ]  │──> SD CS
                │              │
                │  [ GND  ]    │ o GND
                └──────────────┘
```

---

## Tips & troubleshooting
- Ensure a common ground across all modules.
- If a module uses 5V signals, use a proper level shifter on signal lines to protect the ESP32.
- If SD mount fails, check that the card is FAT32-formatted, try a different card, and ensure CS pin
  matches `SD.begin()` in `src/main.cpp`.
- If pins conflict with your board variant, pick other free UART-capable pins and update `src/main.cpp`.

---

If you'd like, I can also add a small photo/diagram file (SVG/PNG) for the README or export a printable
PDF with the wiring diagram.
                                                                                                │  [ GPIO18 ]  │<── SD SCK
