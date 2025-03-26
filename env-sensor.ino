#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <WiFiClientSecure.h>
#include <Wire.h>
#include "Zanshin_BME680.h"  // Same library as before
#include "Adafruit_CCS811.h"
#include "config.h"  // Include configuration header

// Home Assistant configuration
const char* HA_URL_BASE = HA_URL;
const char* HA_TOKEN = HA_TOKEN_STR;
const int STAT_LED = 2;  // Blue LED on the board

// Initialize sensors
BME680_Class BME680;
Adafruit_CCS811 ccs;

void setupBME680() {
  Serial.print(F("- Initializing BME680 sensor\n"));
  while (!BME680.begin(I2C_STANDARD_MODE)) {  // Start BME680 using I2C, use first device found
    Serial.print(F("-  Unable to find BME680. Trying again in 5 seconds.\n"));
    delay(5000);
  }

  Serial.print(F("- Setting 16x oversampling for all sensors\n"));
  BME680.setOversampling(TemperatureSensor, Oversample16);
  BME680.setOversampling(HumiditySensor, Oversample16);
  BME680.setOversampling(PressureSensor, Oversample16);

  Serial.print(F("- Setting IIR filter to a value of 4 samples\n"));
  BME680.setIIRFilter(IIR4);

  Serial.print(F("- Setting gas measurement to 320°C for 150ms\n"));
  BME680.setGas(320, 150);  // 320°C for 150 milliseconds
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
  client.setInsecure();  // For initial testing only - we'll add proper cert verification later
  HTTPClient http;
  
  String url = String(HA_URL_BASE) + entityId;
  http.begin(client, url);
  
  // Add headers
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", String("Bearer ") + HA_TOKEN);

  // Create JSON payload
  StaticJsonDocument<200> doc;
  doc["state"] = value;
  doc["attributes"]["unit_of_measurement"] = unit;
  if (device_class) {
    doc["attributes"]["device_class"] = device_class;
  }
  
  String jsonString;
  serializeJson(doc, jsonString);

  // Send POST request
  int httpResponseCode = http.POST(jsonString);
  
  if (httpResponseCode > 0) {
    digitalWrite(STAT_LED, HIGH);  // Success indication
    Serial.printf("HTTP Response code: %d for %s\n", httpResponseCode, entityId);
  } else {
    digitalWrite(STAT_LED, LOW);   // Error indication
    Serial.printf("Error code: %d for %s\n", httpResponseCode, entityId);
  }

  http.end();
}

void setup() {
  // Initialize serial communication
  Serial.begin(115200);
  while(!Serial) delay(10);  // Wait for serial to be ready
  
  // Configure status LED
  pinMode(STAT_LED, OUTPUT);
  digitalWrite(STAT_LED, LOW);
  
  // Initialize I2C with pins verified by continuity testing
  Wire.begin(15, 5);  // SDA = GPIO15, SCL = GPIO5
  Wire.setClock(100000);  // Set to standard 100kHz I2C speed
  
  // Initialize sensors
  setupBME680();
  setupCCS811();
  
  // Connect to WiFi
  Serial.print("Connecting to ");
  Serial.println(WIFI_SSID);
  
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  
  // Flash LED while connecting
  while (WiFi.status() != WL_CONNECTED) {
    digitalWrite(STAT_LED, !digitalRead(STAT_LED));
    delay(500);
    Serial.print(".");
  }
  
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void loop() {
  static int32_t temp, humidity, pressure, gas;  // BME readings
  static char buf[16];  // For formatting floats
  static float humi_f, temp_f;  // For CCS811 compensation
  
  // Get readings from BME680
  if (BME680.getSensorData(temp, humidity, pressure, gas)) {
    // Adjust temperature and humidity based on previous calibration
    temp = temp - 300;      // Subtract 3°C (300 = 3.00°C)
    humidity = humidity + 7000;  // Add 7% (7000 = 7.000%)
    
    // Send BME680 measurements to Home Assistant
    sendToHomeAssistant("temperature", (float)temp / 100.0, "°C", "temperature");
    sendToHomeAssistant("humidity", (float)humidity / 1000.0, "%", "humidity");
    sendToHomeAssistant("pressure", (float)pressure / 100.0, "hPa", "pressure");
    sendToHomeAssistant("gas", (float)gas / 100.0, "kΩ");
    
    // Format temperature and humidity for CCS811
    sprintf(buf, "%3d.%02d", (int8_t)(humidity / 1000), (uint16_t)(humidity % 1000));
    humi_f = strtod(buf, NULL);
    sprintf(buf, "%3d.%02d", (int8_t)(temp / 100), (uint8_t)(temp % 100));
    temp_f = strtod(buf, NULL);
    
    // Set environmental data for CCS811 compensation
    ccs.setEnvironmentalData(humi_f, temp_f);
  }
  
  // Read CCS811 data
  if (ccs.available() && !ccs.readData()) {
    uint16_t eco2 = ccs.geteCO2();
    uint16_t tvoc = ccs.getTVOC();
    
    // Send CCS811 measurements to Home Assistant
    sendToHomeAssistant("co2", (float)eco2, "ppm", "carbon_dioxide");
    sendToHomeAssistant("tvoc", (float)tvoc, "ppb");
    
    Serial.printf("CO2: %d ppm, TVOC: %d ppb\n", eco2, tvoc);
  }
  
  delay(5000);  // Wait 5 seconds between readings
} 