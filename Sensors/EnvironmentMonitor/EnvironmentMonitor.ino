#include "localconfig.h"

#define MAXTIMEOUT 20 * 1000  // milliseconds (20 s)
#define MAXMESSAGELENGTH 100  // characters
#define SLEEPTIME 20 * 60 // seconds (20 m)

#ifdef LOCALCONFIG
const char *ssid = LC_SSID;
const char *password = LC_SSIDPW;
const char *reportHost = LC_REPORTHOST;
const int reportPort = LC_REPORTPORT;
const char *reportUri = LC_REPORTURI;
#else
const char *ssid = "SSID";
const char *password = "PW";
const char *reportHost = "webserver.invalid;
const int reportPort = 80;
const char *reportUri = "/report";
#endif

#include <nvs.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>

int start = millis();
bool serialInitialized = false;

uint32_t nvshandle;
char *id = "unknown\0";
char message[MAXMESSAGELENGTH];

#define BME_SCK 18
#define BME_MOSI 19
#define BME_MISO 23
#define BME_CS 17
Adafruit_BME280 bme(BME_SCK, BME_MOSI, BME_MISO, BME_CS);
bool bmeInitialized = false;

#define ACTIVE_LED 4

#define ANALOG_MAXVALUE 4095.0
#define DUST_READ 32
#define DUST_LED 16
#define DUST_SAMPLETIME 280
#define DUST_PULSETIME 320
#define DUST_CYCLETIME 10000

#define MAXLOOPCOUNT 10
float ppm = 0.0;

void initSerial() {
#ifdef DEBUG
  Serial.begin(115200);
  delay(50);
  serialInitialized = true;
#endif
}

void printSerial(char *message) {
#ifdef DEBUG
  if (serialInitialized) {
    Serial.print(message);
  }
#endif
}

void showActivity() {
  digitalWrite(ACTIVE_LED, LOW);
}

void showSleep() {
  digitalWrite(ACTIVE_LED, HIGH);
}

void deepSleep() {
  int wakeupInterval = ((SLEEPTIME * 1000) - millis()) * 1000;
  snprintf(message, MAXMESSAGELENGTH, "\nsleeping for %i seconds\n", wakeupInterval / 1000 / 1000);
  printSerial(message);

  showSleep();
  esp_sleep_enable_timer_wakeup(wakeupInterval);
  esp_deep_sleep_start();
}

void getChipId(char *const chipId) {
  uint64_t chipIdNum = ESP.getEfuseMac();
  char *mac = (char*)malloc(13);
  snprintf(mac, 13, "%" PRIx64, chipIdNum);
  for (int i = 0; i <= 5; i++) {
    strncpy(&chipId[i * 3], &mac[i * 2], 2);
    chipId[i * 3 + 2] = i < 5 ? ':' : '\0';
  }
  free(mac);
}

bool initNonVolatile() {
  if (!nvshandle) {
    if (nvs_open("storage", NVS_READWRITE, &nvshandle)) {
      printSerial("failed to access non-volatile storage");
      return false;
    }
  }

  size_t required_size;
  if (nvs_get_str(nvshandle, "unitid", NULL, &required_size) == ESP_OK) {
    id = new char(required_size);
    nvs_get_str(nvshandle, "unitid", id, &required_size);
    snprintf(message, MAXMESSAGELENGTH, "unit ID %s retrieved from storage\n", id);
    printSerial(message);
  } else {
    id = (char*)malloc(18);
    getChipId(id);
    snprintf(message, MAXMESSAGELENGTH, "failed to read unit ID from storage, using chip ID %s\n", id);
    printSerial(message);
  }

  return true;
}

void initIO() {
  pinMode(ACTIVE_LED, OUTPUT);
  pinMode(DUST_LED, OUTPUT);
  pinMode(DUST_READ, INPUT);
}

bool initSensors() {
  showActivity();
  if (!bmeInitialized && !bme.begin()) {
    printSerial("unable to initialize BME280\n");
    return false;
  }
  bmeInitialized = true;
  return bmeInitialized;
}

bool connectWifi() {
  snprintf(message, MAXMESSAGELENGTH, "connecting to %s...", ssid);
  printSerial(message);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - start > MAXTIMEOUT) {
      printSerial(" failed to connect\n");
      return false;
    }
    delay(200);
    printSerial(".");
  }

  IPAddress localIP = WiFi.localIP();
  snprintf(message, MAXMESSAGELENGTH, " done, local IP = %d.%d.%d.%d\n", localIP[0], localIP[1], localIP[2], localIP[3]);
  printSerial(message);

  return true;
}

float getTemperature() {
  return bmeInitialized ? bme.readTemperature() : 0.0f;
}

int getPressure() {
  return bmeInitialized ? bme.readPressure() : 0;
}

int getHumidity() {
  return bmeInitialized ? bme.readHumidity() : 0;
}

float getParticleCount() {
  return readDustSensor() / (float)MAXLOOPCOUNT;
}

char *getJsonData() {
  StaticJsonBuffer<512> jsonBuffer;
  JsonObject &root = jsonBuffer.createObject();
  root["sensorId"] = id;
  root["temperature"] = getTemperature();
  root["pressure"] = getPressure();
  root["humidity"] = getHumidity();
  root["particles"] = getParticleCount();
  size_t jsonMessageLength = root.measureLength() + 1;
  char *jsonMessage = (char*)malloc(jsonMessageLength);
  root.printTo(jsonMessage, jsonMessageLength);
  return jsonMessage;
}

bool reportSensorReadings() {
  bool result = false;

  snprintf(message, MAXMESSAGELENGTH, "connecting to %s:%d%s...", reportHost, reportPort, reportUri);
  printSerial(message);

  HTTPClient httpClient;
  httpClient.begin(reportHost, reportPort, reportUri);
  httpClient.addHeader("Content-Type", "application/json");
  int responseCode = httpClient.POST(getJsonData());
  if (responseCode == 200) {
    snprintf(message, MAXMESSAGELENGTH, " success, response code %d\n\n%s\n", responseCode, httpClient.getString().c_str());
    printSerial(message);
    result = true;
  } else if (responseCode > 0) {
    snprintf(message, MAXMESSAGELENGTH, " failed, response code %d\n%s", responseCode, httpClient.getString().c_str());
    printSerial(message);
  } else {
    snprintf(message, MAXMESSAGELENGTH, " failed, response code %d\n", responseCode);
    printSerial(message);
  }
  httpClient.end();
  return result;
}

void dustLedOn() {
  digitalWrite(DUST_LED, LOW);
}

void dustLedOff() {
  digitalWrite(DUST_LED, HIGH);
}

float getDustSensorSample() {
  dustLedOn();
  delayMicroseconds(DUST_SAMPLETIME);
  int raw = analogRead(DUST_READ);
  delayMicroseconds(DUST_PULSETIME - DUST_SAMPLETIME);
  dustLedOff();
  float voltage = 3.3 * (raw / (float)ANALOG_MAXVALUE);
  float density = fmax(0.0, 0.17 * voltage - 0.1);
  //snprintf(message, MAXMESSAGELENGTH, "[ r = %d, v = %f, d = %f ]\n", raw, voltage, density);
  //printSerial(message);
  return density;
}

float readDustSensor() {
  //printSerial("activating dust sensor\n");
  getDustSensorSample(); // warm up, first read is always 0
  delay(10);
  for (int i = 0; i <= MAXLOOPCOUNT; i++) {
    ppm += getDustSensorSample();
    delayMicroseconds(DUST_CYCLETIME - DUST_PULSETIME);
  }
  //snprintf(message, MAXMESSAGELENGTH, "dust density value is %f after %d loops\n", ppm, MAXLOOPCOUNT);
  //printSerial(message);
  return ppm;
}

void setup()
{
  initSerial();
  initIO();
  if (initSensors() && initNonVolatile() && connectWifi()) {
    reportSensorReadings();
  }
  deepSleep();
}

void loop() { }
