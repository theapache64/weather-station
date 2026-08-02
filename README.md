# weather-station

ESP32 weather station. Reads temperature and humidity from a DHT22, computes a comfort score, then reports to Google Forms and Telegram on a timer.

## Hardware

- ESP32 DevKit (e.g. DOIT ESP32 DEVKIT V1)
- DHT22

**Wiring (DHT22 → ESP32)**

| DHT22 | ESP32   |
|-------|---------|
| VCC   | 3V3     |
| GND   | GND     |
| DATA  | GPIO 5  |

## Setup

1. Copy secrets: create `src/Keys.cpp` from `Keys.h` (Wi‑Fi, Telegram bot, Google Form/Sheet URLs). That file is gitignored.
2. Flash with PlatformIO:
   ```bash
   pio run -t upload
   pio device monitor -b 115200
   ```

## What it does

1. Connects to Wi‑Fi  
2. Loads config from a Google Sheet CSV (sleep interval + score weights)  
3. Reads DHT22  
4. POSTs temp / humidity / score to a Google Form  
5. Sends a short Telegram summary  
6. Sleeps, then repeats  

## License

Apache License 2.0 — see license header in source / project if added.
