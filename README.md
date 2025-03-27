# ESP32 Environmental Sensor

An ESP32-based environmental monitoring system that measures temperature, humidity, pressure, gas resistance, CO2, and TVOC levels, sending the data to Home Assistant.

## Hardware

- SparkFun ESP32 Qwiic Pro Mini (DEV-23386)
  - ESP32-PICO-MINI-02-N8R2 microcontroller
  - 8MB Flash, 2MB PSRAM
  - Dual-core processor (80-240MHz)
  - Built-in WiFi and Bluetooth
  - Qwiic connector for I2C sensors
  - [Product Page](https://www.sparkfun.com/products/23386)
  - [Hookup Guide](https://sparkfun.github.io/SparkFun_ESP32_Qwiic_Pro_Mini/)
  - [GitHub Repository](https://github.com/sparkfun/SparkFun_ESP32_Qwiic_Pro_Mini)
- BME680 environmental sensor (via Qwiic)
  - Temperature
  - Humidity
  - Barometric pressure
  - Gas resistance
- CCS811 air quality sensor (via Qwiic)
  - eCO2 (equivalent CO2)
  - TVOC (Total Volatile Organic Compounds)
- PMS5003 particulate matter sensor
  - PM1.0, PM2.5, PM10 measurements
  - Particle counting

## Dependencies

- [Zanshin BME680 Library](https://github.com/Zanduino/BME680)
- [Adafruit CCS811 Library](https://github.com/adafruit/Adafruit_CCS811)
- ArduinoJson
- WiFi, HTTPClient, Wire (built into ESP32 Arduino core)

## Setup

1. Copy `config.h.example` to `config.h`
2. Edit `config.h` with your:
   - WiFi credentials
   - Home Assistant URL
   - Home Assistant Long-Lived Access Token

## Sensor Configuration

- BME680:
  - 16x oversampling for temperature, humidity, and pressure
  - IIR filter set to 4 samples
  - Gas sensor: 320°C for 150ms
  - Temperature offset: -3°C
  - Humidity offset: +7%

- Readings are sent every 10 seconds to Home Assistant

## Home Assistant Integration

Sensors are exposed as the following entities:

- `sensor.env_temperature` (°C)
- `sensor.env_humidity` (%)
- `sensor.env_pressure` (hPa)
- `sensor.env_gas` (kΩ)
- `sensor.env_co2` (ppm)
- `sensor.env_tvoc` (ppb)
