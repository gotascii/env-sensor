#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <WiFiClientSecure.h>
#include <Wire.h>
#include "Zanshin_BME680.h"
#include "Adafruit_CCS811.h"
#include "config.h"

// Pin Configuration
const int PIN_I2C_SDA = 15;       // I2C SDA pin
const int PIN_I2C_SCL = 5;        // I2C SCL pin
const int PIN_PMS_RX = 7;         // GPIO7 (TXO pin) for receiving PMS5003 data

// Timing Constants
const unsigned long SERIAL_BAUD = 115200;
const unsigned long PMS_BAUD = 9600;
const unsigned long SENSOR_READ_INTERVAL = 30000;  // ms between readings
const unsigned long WIFI_STATUS_INTERVAL = 500;    // ms between WiFi init status check
const unsigned long BME680_RETRY_INTERVAL = 5000;  // ms between BME680 init retries

// BME680 Configuration
const int BME680_HEATER_TEMP = 320;     // Gas heater temperature in °C
const int BME680_HEATER_TIME = 150;     // Gas heater time in ms
const int BME680_TEMP_OFFSET = -300;    // Temperature offset (3°C * -100)
const int BME680_HUMIDITY_OFFSET = 7000; // Humidity offset (7%)

// Home Assistant configuration
const char* HA_URL_BASE = HA_URL;
const char* HA_TOKEN = HA_TOKEN_STR;

// PMS5003 Data Structure
struct pms5003data {
  uint16_t framelen;
  uint16_t pm10_standard, pm25_standard, pm100_standard;
  uint16_t pm10_env, pm25_env, pm100_env;
  uint16_t particles_03um, particles_05um, particles_10um;
  uint16_t particles_25um, particles_50um, particles_100um;
  uint16_t unused;
  uint16_t checksum;
};

// Initialize sensors
BME680_Class BME680;
Adafruit_CCS811 ccs;
struct pms5003data pmsData;

void setupBME680() {
  Serial.print(F("- Initializing BME680 sensor\n"));
  while (!BME680.begin(I2C_STANDARD_MODE)) {
    Serial.print(F("-  Unable to find BME680. Trying again in 5 seconds.\n"));
    delay(BME680_RETRY_INTERVAL);
  }

  Serial.print(F("- Setting 16x oversampling for all sensors\n"));
  BME680.setOversampling(TemperatureSensor, Oversample16);
  BME680.setOversampling(HumiditySensor, Oversample16);
  BME680.setOversampling(PressureSensor, Oversample16);

  Serial.print(F("- Setting IIR filter to a value of 4 samples\n"));
  BME680.setIIRFilter(IIR4);

  Serial.print(F("- Setting gas measurement to 320°C for 150ms\n"));
  BME680.setGas(BME680_HEATER_TEMP, BME680_HEATER_TIME);
}

void setupCCS811() {
  if(!ccs.begin()){
    Serial.println("Failed to start CCS811! Please check your wiring.");
    while(1);
  }

  while(!ccs.available());
}

void setupPMS5003() {
  Serial1.begin(PMS_BAUD, SERIAL_8N1, PIN_PMS_RX, -1);  // -1 for TX pin as we don't need it
  Serial.println("PMS5003 initialized on pin " + String(PIN_PMS_RX));
}

boolean readPMSdata() {
  if (!Serial1.available()) {
    Serial.println("No PMS data available");
    return false;
  }

  Serial.printf("PMS bytes available: %d\n", Serial1.available());
  boolean validPacketFound = false;

  // Process all complete packets in the buffer
  while (Serial1.available() >= 32) {  // Need at least 32 bytes for a complete packet
    // Look for start byte
    if (Serial1.peek() != 0x42) {
      uint8_t skipped = Serial1.read();
      Serial.printf("Skipping byte: 0x%02X\n", skipped);
      continue;
    }

    uint8_t buffer[32];    
    uint16_t sum = 0;
    Serial1.readBytes(buffer, 32);

    // Debug print the raw buffer
    Serial.print("Raw buffer: ");
    for (int i = 0; i < 32; i++) {
      Serial.printf("0x%02X ", buffer[i]);
      if (i == 15) Serial.print("\n            ");  // Line break for readability
    }
    Serial.println();

    // Calculate checksum
    for (uint8_t i = 0; i < 30; i++) {
      sum += buffer[i];
    }

    // Convert endianness
    uint16_t buffer_u16[15];
    for (uint8_t i = 0; i < 15; i++) {
      buffer_u16[i] = buffer[2 + i*2 + 1];
      buffer_u16[i] += (buffer[2 + i*2] << 8);
    }

    // Copy to our struct
    struct pms5003data currentData;
    memcpy((void *)&currentData, (void *)buffer_u16, 30);

    if (sum == currentData.checksum) {
      // Valid packet found, update our global data
      memcpy((void *)&pmsData, (void *)&currentData, sizeof(pms5003data));
      validPacketFound = true;
      Serial.println("Valid PMS packet processed!");
    } else {
      Serial.printf("Checksum failure - calculated: 0x%04X, received: 0x%04X\n", sum, currentData.checksum);
    }
  }

  // If there's data left but less than 32 bytes, log it
  if (Serial1.available() > 0) {
    Serial.printf("%d incomplete bytes left in buffer\n", Serial1.available());
  }
  
  return validPacketFound;
}

void sendToHomeAssistant(const char* entityId, float value, const char* unit, const char* device_class = nullptr) {
  if (WiFi.status() != WL_CONNECTED) {
    return;
  }

  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  
  String url = String(HA_URL_BASE) + entityId;
  http.begin(client, url);
  
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", String("Bearer ") + HA_TOKEN);

  StaticJsonDocument<200> doc;
  doc["state"] = value;
  doc["attributes"]["unit_of_measurement"] = unit;
  if (device_class) {
    doc["attributes"]["device_class"] = device_class;
  }
  
  String jsonString;
  serializeJson(doc, jsonString);

  int httpResponseCode = http.POST(jsonString);
  
  if (httpResponseCode > 0) {
    Serial.printf("HTTP Response code: %d for %s\n", httpResponseCode, entityId);
  } else {
    Serial.printf("Error code: %d for %s\n", httpResponseCode, entityId);
  }

  http.end();
}

void sendPMSDataToHomeAssistant() {
  sendToHomeAssistant("pm1", (float)pmsData.pm10_standard, "µg/m³", "pm1");
  sendToHomeAssistant("pm25", (float)pmsData.pm25_standard, "µg/m³", "pm25");
  sendToHomeAssistant("pm10", (float)pmsData.pm100_standard, "µg/m³", "pm10");
  
  sendToHomeAssistant("particles_03um", (float)pmsData.particles_03um, "particles/0.1L");
  sendToHomeAssistant("particles_05um", (float)pmsData.particles_05um, "particles/0.1L");
  sendToHomeAssistant("particles_10um", (float)pmsData.particles_10um, "particles/0.1L");
  sendToHomeAssistant("particles_25um", (float)pmsData.particles_25um, "particles/0.1L");
  sendToHomeAssistant("particles_50um", (float)pmsData.particles_50um, "particles/0.1L");
  sendToHomeAssistant("particles_100um", (float)pmsData.particles_100um, "particles/0.1L");
}

void setup() {
  Serial.begin(SERIAL_BAUD);
  while(!Serial) delay(10);
  
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  
  setupBME680();
  setupCCS811();
  setupPMS5003();
  
  Serial.print("Connecting to ");
  Serial.println(WIFI_SSID);
  
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(WIFI_STATUS_INTERVAL);
    Serial.print(".");
  }
  
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void loop() {
  static int32_t temp, humidity, pressure, gas;
  static char buf[16];
  static float humi_f, temp_f;
  
  // Read PMS data if available
  if (readPMSdata()) {
    sendPMSDataToHomeAssistant();
    Serial.printf("PM 1.0: %d, PM 2.5: %d, PM 10: %d\n",
      pmsData.pm10_standard,
      pmsData.pm25_standard,
      pmsData.pm100_standard);
  }
  
  if (BME680.getSensorData(temp, humidity, pressure, gas)) {
    temp = temp + BME680_TEMP_OFFSET;
    humidity = humidity + BME680_HUMIDITY_OFFSET;
    
    sendToHomeAssistant("temperature", (float)temp / 100.0, "°C", "temperature");
    sendToHomeAssistant("humidity", (float)humidity / 1000.0, "%", "humidity");
    sendToHomeAssistant("pressure", (float)pressure / 100.0, "hPa", "pressure");
    sendToHomeAssistant("gas", (float)gas / 100.0, "kΩ");
    
    sprintf(buf, "%3d.%02d", (int8_t)(humidity / 1000), (uint16_t)(humidity % 1000));
    humi_f = strtod(buf, NULL);
    sprintf(buf, "%3d.%02d", (int8_t)(temp / 100), (uint8_t)(temp % 100));
    temp_f = strtod(buf, NULL);
    
    ccs.setEnvironmentalData(humi_f, temp_f);
  }
  
  if (ccs.available() && !ccs.readData()) {
    uint16_t eco2 = ccs.geteCO2();
    uint16_t tvoc = ccs.getTVOC();
    
    sendToHomeAssistant("co2", (float)eco2, "ppm", "carbon_dioxide");
    sendToHomeAssistant("tvoc", (float)tvoc, "ppb");
    
    Serial.printf("CO2: %d ppm, TVOC: %d ppb\n", eco2, tvoc);
  }
  
  delay(SENSOR_READ_INTERVAL);
} 