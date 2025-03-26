// The idea is that CCS uses the humi and temp data from the BME680
// But both have hot plates so I have no idea why ^

#include <WiFi.h>
#include "Zanshin_BME680.h"
#include "Adafruit_CCS811.h"
#include <SoftwareSerial.h>
SoftwareSerial pmsSerial(2, 3);

#define WIFI_SSID "The Black Lodge"
#define WIFI_PASSWORD ""

unsigned long ts = 0;

char serverAddress[] = "10.0.1.9";
int port = 8080;

WiFiClient wifi;

const uint32_t SERIAL_SPEED{115200};

Adafruit_CCS811 ccs;

BME680_Class BME680;

struct pms5003data {
  uint16_t framelen;
  uint16_t pm10_standard, pm25_standard, pm100_standard;
  uint16_t pm10_env, pm25_env, pm100_env;
  uint16_t particles_03um, particles_05um, particles_10um, particles_25um, particles_50um, particles_100um;
  uint16_t unused;
  uint16_t checksum;
};
struct pms5003data data;

float altitude(const int32_t press, const float seaLevel = 1013.25);
float altitude(const int32_t press, const float seaLevel) {  /*!
  @brief     This converts a pressure measurement into a height in meters
  @details   The corrected sea-level pressure can be passed into the function if it is known,
             otherwise the standard atmospheric pressure of 1013.25hPa is used (see
             https://en.wikipedia.org/wiki/Atmospheric_pressure) for details.
  @param[in] press    Pressure reading from BME680
  @param[in] seaLevel Sea-Level pressure in millibars
  @return    floating point altitude in meters.
  */
  static float Altitude;
  // Convert into meters
  Altitude = 44330.0 * (1.0 - pow(((float)press / 100.0) / seaLevel, 0.1903));
  return (Altitude);
}

void setupWifi() {
  Serial.print("Connecting to '");
  Serial.print(WIFI_SSID);
  Serial.print("' ...");

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

}

void setupBME680() {
  Serial.print(F("- Initializing BME680 sensor\n"));
  while (!BME680.begin(I2C_STANDARD_MODE)) {
    Serial.print(F("-  Unable to find BME680. Trying again in 5 seconds.\n"));
    delay(5000);
  }

  Serial.print(F("- Setting 16x oversampling for all sensors\n"));
  BME680.setOversampling(TemperatureSensor, Oversample16);
  BME680.setOversampling(HumiditySensor, Oversample16);
  BME680.setOversampling(PressureSensor, Oversample16);

  Serial.print(F("- Setting IIR filter to a value of 4 samples\n"));
  BME680.setIIRFilter(IIR4);

  // "°C" symbols
  Serial.print(F("- Setting gas measurement to 320\xC2\xB0\x43 for 150ms\n"));
  // 320°C for 150 milliseconds
  BME680.setGas(320, 150);
  
}

void setupCCS811() {
  if(!ccs.begin()){
    Serial.println("Failed to start CCS811! Please check your wiring.");
    while(1);
  }

  while(!ccs.available());
}

boolean readPMSdata(Stream *s) {
  if (! s->available()) {
    return false;
  }

  // Read a byte at a time until we get to the special '0x42' start-byte
  if (s->peek() != 0x42) {
    s->read();
    return false;
  }

  // Now read all 32 bytes
  if (s->available() < 32) {
    return false;
  }

  uint8_t buffer[32];    
  uint16_t sum = 0;
  s->readBytes(buffer, 32);

  // get checksum ready
  for (uint8_t i=0; i<30; i++) {
    sum += buffer[i];
  }

  /* debugging
  for (uint8_t i=2; i<32; i++) {
    Serial.print("0x"); Serial.print(buffer[i], HEX); Serial.print(", ");
  }
  Serial.println();
  */

  // The data comes in endian'd, this solves it so it works on all platforms
  uint16_t buffer_u16[15];
  for (uint8_t i=0; i<15; i++) {
    buffer_u16[i] = buffer[2 + i*2 + 1];
    buffer_u16[i] += (buffer[2 + i*2] << 8);
  }

  // put it into a nice struct :)
  memcpy((void *)&data, (void *)buffer_u16, 30);

  if (sum != data.checksum) {
    Serial.println("Checksum failure");
    return false;
  }
  // success!
  return true;
}

void setup() {
  Serial.begin(SERIAL_SPEED);
  setupWifi();
  setupBME680();
  setupCCS811();
  pmsSerial.begin(9600);
}

void sendMetrics() {
  if (!wifi.connected()) {
    wifi.flush();
    wifi.stop();
    if (!wifi.connect(serverAddress, port)) {
      Serial.println("Cannot connect to go server! Waiting");
      delay(1000);
      return;
    }
  }

  static int32_t  temp, humidity, pressure, gas;  // BME readings
  static char     buf[16];                        // sprintf text buffer
  static float    alt;                            // Temporary variable
  static uint16_t loopCounter = 0;                // Display iterations
  static uint16_t ecotwo, tvoc;
  static float humi_f, temp_f;

  sprintf(buf, "%i,", data.pm10_standard);
  wifi.print(buf);
  sprintf(buf, "%i,", data.pm25_standard);
  wifi.print(buf);
  sprintf(buf, "%i,", data.pm100_standard);
  wifi.print(buf);
  sprintf(buf, "%i,", data.pm10_env);
  wifi.print(buf);
  sprintf(buf, "%i,", data.pm25_env);
  wifi.print(buf);
  sprintf(buf, "%i,", data.pm100_env);
  wifi.print(buf);
  sprintf(buf, "%i,", data.particles_03um);
  wifi.print(buf);
  sprintf(buf, "%i,", data.particles_05um);
  wifi.print(buf);
  sprintf(buf, "%i,", data.particles_10um);
  wifi.print(buf);
  sprintf(buf, "%i,", data.particles_25um);
  wifi.print(buf);
  sprintf(buf, "%i,", data.particles_50um);
  wifi.print(buf);
  sprintf(buf, "%i,", data.particles_100um);
  wifi.print(buf);

  // First, get the BME data and adjust temp and humi to account for weird shit
  BME680.getSensorData(temp, humidity, pressure, gas);
  temp = temp - 300;
  humidity = humidity + 7000;

  // Then put that data into a format that CCS can use
  sprintf(buf, "%3d.%02d", (int8_t)(humidity / 1000), (uint16_t)(humidity % 1000));
  humi_f = strtod(buf, NULL);
  sprintf(buf, "%3d.%02d", (int8_t)(temp / 100), (uint8_t)(temp % 100));
  temp_f = strtod(buf, NULL);

  // Then feed that data into CCS
  ccs.setEnvironmentalData(humi_f, temp_f);  
  
  // Then read CCS data
  if (ccs.available() && !ccs.readData()) {
    ecotwo = ccs.geteCO2();
    tvoc = ccs.getTVOC();
  }

  // Write CCS data
  sprintf(buf, "%i,", tvoc);
  wifi.print(buf);
  sprintf(buf, "%i,", ecotwo);
  wifi.print(buf);

  // Write offset-adjusted BME data
  sprintf(buf, "%d.%02d,", (int8_t)(temp / 100), (uint8_t)(temp % 100));
  wifi.print(buf);

  sprintf(buf, "%d.%03d,", (int8_t)(humidity / 1000), (uint16_t)(humidity % 1000));
  wifi.print(buf);

  sprintf(buf, "%d.%02d,", (int16_t)(pressure / 100), (uint8_t)(pressure % 100));
  wifi.print(buf);

  // Air (resistance in) m(illiohms)
  sprintf(buf, "%d.%02d", (int16_t)(gas / 100), (uint8_t)(gas % 100));
  wifi.print(buf);

  // Alt(itude) (m)eters
  // alt = altitude(pressure);
  // sprintf(buf, "%5d.%02d", (int16_t)(alt), ((uint8_t)(alt * 100) % 100));
  // Serial.print(buf);

  wifi.println();
}

void loop() {
  // readPMSdata has to run at loop speed, not sure why
  if(readPMSdata(&pmsSerial) && ((millis() - ts) > 5000)) {
    ts = millis();
    Serial.println("Sending");
    sendMetrics();
  }
}

//if (wifi.connect(serverAddress, port)) {
//  Serial.println("Connected to server");
//  wifi.println("GET /m?q=arduino HTTP/1.0");
//
//  while(wifi.connected())
//  {
//    while(wifi.available() > 0)
//    {
//      char c = wifi.read();
//      Serial.print(c);
//    }
//  }
//
//  if(!wifi.connected()) {
//    // if the server's disconnected, stop the client:
//    Serial.println("disconnected");
//    wifi.flush();
//    wifi.stop();
//  }
//} else {
//  Serial.println("connection failed");
//}
