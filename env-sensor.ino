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

// Timing Constants
const unsigned long SERIAL_BAUD = 115200;
const unsigned long SENSOR_READ_INTERVAL = 5000;  // ms between readings
const unsigned long WIFI_STATUS_INTERVAL = 500;   // ms between WiFi status blinks
const unsigned long BME680_RETRY_INTERVAL = 5000; // ms between BME680 init retries

// BME680 Configuration
const int BME680_HEATER_TEMP = 320;     // Gas heater temperature in °C
const int BME680_HEATER_TIME = 150;     // Gas heater time in ms
const int BME680_TEMP_OFFSET = -300;    // Temperature offset (3°C * -100)
const int BME680_HUMIDITY_OFFSET = 7000; // Humidity offset (7%)

// Home Assistant configuration
const char* HA_URL_BASE = HA_URL;
const char* HA_TOKEN = HA_TOKEN_STR;

// Initialize sensors
BME680_Class BME680;
Adafruit_CCS811 ccs;

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

void setup() {
  Serial.begin(SERIAL_BAUD);
  while(!Serial) delay(10);
  
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  
  setupBME680();
  setupCCS811();
  
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