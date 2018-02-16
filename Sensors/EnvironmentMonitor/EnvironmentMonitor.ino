#include "localconfig.h"

#include <string.h>
#include <time.h>
#include <nvs.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>

#define MAXTIMEOUT 20 * 1000  // milliseconds (20 s)
#define MAXMESSAGELENGTH 100  // characters
#define TZOFFSET 1 * 60 * 60  // seconds (2 h)
#define DSTOFFSET 1 * 60 * 60 // seconds (1 h)
#define SLEEPTIME 15 * 60 // seconds (15 m)

#ifdef LOCALCONFIG
const char *ssid     = LC_SSID;
const char *password = LC_SSIDPW;
const char *mqttHost = LC_MQTTHOST;
const int mqttPort = LC_MQTTPORT;
const char *mqttUser = LC_MQTTUSER;
const char *mqttPassword = LC_MQTTPW;
const char *mqttTopic = LC_MQTTTOPIC;
const char *reportHost = LC_REPORTHOST;
const int reportPort = LC_REPORTPORT;
const char *ntpHost = LC_NTPHOST;
#else
const char *ssid     = "SSID";
const char *password = "PW";
const char *mqttHost = "mqtt.org";
const int mqttPort = 12345;
const char *mqttUser = "user";
const char *mqttPassword = "pw";
const char *mqttTopic = "esp/test";
const char *reportHost = "webserver.org";
const int reportPort = 80;
const char *ntpHost = "pool.ntp.org";
#endif

uint32_t nvshandle;
String id = "";
char message[MAXMESSAGELENGTH];
int start = millis();

void initSerial() {
#ifdef DEBUG
  Serial.begin(115200);
  delay(50);
#endif
}

void printSerial(String message) {
#ifdef DEBUG
  Serial.print(message);
#endif
}

String getChipId() {
  uint64_t chipIdNum = ESP.getEfuseMac();
  char mac[12];
  sprintf(mac, "%" PRIx64, chipIdNum);
  char chipId[18];
  for (int i = 0; i <= 5; i++) {
    strncpy(&chipId[i * 3], &mac[i * 2], 2);
    chipId[i * 3 + 2] = i < 5 ? ':' : '\0';
  }
  return String(chipId);
}

bool initNonVolatile() {
  if (!nvshandle) {
    if (nvs_open("storage", NVS_READWRITE, &nvshandle)) {
      printSerial("failed to access non-volatile storage");
      return false;
    }
  }

  size_t required_size;
  nvs_get_str(nvshandle, "unitid", NULL, &required_size);
  char *unitid = (char*)malloc(required_size);
  if (nvs_get_str(nvshandle, "unitid", unitid, &required_size)) {
    id = getChipId();
    snprintf(message, MAXMESSAGELENGTH, "failed to read unit ID from storage, using chip ID %s\n", id.c_str());
    printSerial(message);
  } else {
    snprintf(message, MAXMESSAGELENGTH, "unit ID %s retrieved from storage\n", unitid);
    printSerial(message);
    id = String(unitid);
  }

  return true;
}

float getTemperature() {
  float result = 20.1;
  return result;
}

time_t getTimestamp() {
  time_t timestamp;
  time(&timestamp);
  return timestamp;
}

char *getJsonData() {
  StaticJsonBuffer<400> jsonBuffer;
  JsonObject &root = jsonBuffer.createObject();
  root["senderId"] = id;
  root["temperature"] = getTemperature();
  root["timestamp"] = getTimestamp();
  size_t jsonMessageLength = root.measureLength() + 1;
  char *jsonMessage = (char*)malloc(jsonMessageLength);
  root.printTo(jsonMessage, jsonMessageLength);
  return jsonMessage;
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

bool configureTime() {
  printSerial("configuring time...");

  struct tm timeInfo;
  configTime(TZOFFSET, DSTOFFSET, ntpHost);
  while (!getLocalTime(&timeInfo)) {
    if (millis() - start > MAXTIMEOUT) {
      printSerial(" failed to sync time\n");
      return false;
    }
    delay(10);
    printSerial(".");
  }

  strftime(message, MAXMESSAGELENGTH, " done, time is %c\n", &timeInfo);
  printSerial(message);

  return true;
}

bool publishSensorReadings() {
  WiFiClient wifiClient;
  PubSubClient mqttClient(wifiClient);
  mqttClient.setServer(mqttHost, mqttPort);

  snprintf(message, MAXMESSAGELENGTH, "connecting to MQTT at %s:%d...", mqttHost, mqttPort);
  printSerial(message);
  
  while (!mqttClient.connected()) {
    if (millis() - start > MAXTIMEOUT) {
      snprintf(message, MAXMESSAGELENGTH, " failed with state %d\n", mqttClient.state());
      printSerial(message);
      return false;
    }

    if (mqttClient.connect(id.c_str(), mqttUser, mqttPassword )) {
      printSerial(" connected, sending message\n");
      bool result = mqttClient.publish(mqttTopic, getJsonData());
      mqttClient.disconnect();
      return result;
    } else {
      delay(200);
      printSerial(".");
    }
  }

  return false;
}

bool reportSensorReadings() {
  snprintf(message, MAXMESSAGELENGTH, "connecting to %s:%d...", reportHost, reportPort);
  printSerial(message);

  HTTPClient httpClient;
  httpClient.begin(reportHost, reportPort);
  httpClient.addHeader("Content-Type", "application/json");
  bool result = (httpClient.POST(getJsonData()) > 0);
  httpClient.end();
  return result;
}

void deepSleep() {
  int wakeupInterval = ((SLEEPTIME * 1000) - millis()) * 1000;
  snprintf(message, MAXMESSAGELENGTH, "\nsleeping for %i seconds", wakeupInterval / 1000 / 1000);
  printSerial(message);

  esp_sleep_enable_timer_wakeup(wakeupInterval);
  esp_deep_sleep_start();
}

void setup()
{
  initSerial();
  if (initNonVolatile() && connectWifi() && configureTime()) {
    publishSensorReadings();
    reportSensorReadings();
  }
  deepSleep();
}

void loop() { }
