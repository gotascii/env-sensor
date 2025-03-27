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
  - Programming Interface:
    - SparkFun Serial Basic Breakout - CH340C (USB-C)
    - [Product Page](https://www.sparkfun.com/products/15096)
    - [Hookup Guide](https://learn.sparkfun.com/tutorials/sparkfun-serial-basic-ch340c-hookup-guide)
- SparkFun BME680 Environmental Sensor (Qwiic)
  - Temperature
  - Humidity
  - Barometric pressure
  - Gas resistance
  - [Product Page](https://www.sparkfun.com/products/16466)
  - [Hookup Guide](https://learn.sparkfun.com/tutorials/sparkfun-environmental-sensor-breakout---bme680-qwiic-hookup-guide)
  - [GitHub Repository](https://github.com/sparkfun/SparkFun_BME680_Arduino_Library)
- Adafruit CCS811 Air Quality Sensor (STEMMA QT)
  - eCO2 (equivalent CO2)
  - TVOC (Total Volatile Organic Compounds)
  - [Product Info (Archived)](https://learn.adafruit.com/adafruit-ccs811-air-quality-sensor)
  - [Arduino Library](https://github.com/adafruit/Adafruit_CCS811)
  - Note: This sensor has been discontinued and replaced by the ENS160
- PMS5003 particulate matter sensor
  - PM1.0, PM2.5, PM10 measurements
  - Particle counting
  - Connector: 1.25mm pitch to 2.54mm pitch breakout board included

### Power Distribution

- Adafruit Micro-USB breakout board with mounting holes provides main 5V power
- Power distribution using split connector cables:
  - 5V & ground split:
    - PMS5003 breakout board
    - ESP32

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

## Home Assistant Integration

Sensors are exposed as the following entities:

BME680 Environmental Sensor:

- `sensor.env_temperature` (°C, device_class: temperature)
- `sensor.env_humidity` (%, device_class: humidity)
- `sensor.env_pressure` (hPa, device_class: pressure)
- `sensor.env_gas` (kΩ)

CCS811 Air Quality:

- `sensor.env_co2` (ppm, device_class: carbon_dioxide)
- `sensor.env_tvoc` (ppb)

PMS5003 Particulate Matter:

- `sensor.env_pm1` (µg/m³, device_class: pm1)
- `sensor.env_pm25` (µg/m³, device_class: pm25)
- `sensor.env_pm10` (µg/m³, device_class: pm10)
- `sensor.env_particles_03um` (particles/0.1L)
- `sensor.env_particles_05um` (particles/0.1L)
- `sensor.env_particles_10um` (particles/0.1L)
- `sensor.env_particles_25um` (particles/0.1L)
- `sensor.env_particles_50um` (particles/0.1L)
- `sensor.env_particles_100um` (particles/0.1L)
