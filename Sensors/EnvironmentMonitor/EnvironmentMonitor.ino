#include "localconfig.h"

#include <string.h>
#include <time.h>
#include <nvs.h>
#include <WiFi.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>

#define MAXTIMEOUT 15 * 1000  // milliseconds (15 s)
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
const int reportPort = 1234;
const char *ntpHost = "pool.ntp.org";
#endif

WiFiClient wifiClient;
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

float getTemperature() {
  float result = 20.1;

  return result;
}

time_t getTimestamp() {
  time_t timestamp;
  time(&timestamp);
  return timestamp;
}

char *getMqttMessage() {
  StaticJsonBuffer<200> jsonBuffer;
  JsonObject &root = jsonBuffer.createObject();
  root["senderId"] = id;
  root["temperature"] = getTemperature();
  root["timestamp"] = getTimestamp();
  size_t mqttMessageLength = root.measureLength();
  char *mqttMessage = (char*)malloc(mqttMessageLength);
  root.printTo(mqttMessage, mqttMessageLength);
  return mqttMessage;
}

bool publishSensorReadings() {
  bool result = false;
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
      printSerial(" connected\n");

      if (mqttClient.publish(mqttTopic, getMqttMessage())) {
        result = true;
      }
      mqttClient.disconnect();
    } else {
      delay(200);
      printSerial(".");
    }
  }

  return result;
}

bool reportSensorReadings() {
  snprintf(message, MAXMESSAGELENGTH, "connecting to %s...", reportHost);
  printSerial(message);

  if (!wifiClient.connect(reportHost, reportPort)) {
    printSerial(" connection failed\n");
  } else {
    String url = "/report";
    url += "?station=";
    url += id;
    url += "&temperature=";
    url += getTemperature();
    url += "&timestamp=";
    url += getTimestamp();

    wifiClient.print(String("GET ") + url + " HTTP/1.1\r\n" +
                     "Host: " + reportHost + "\r\n" +
                     "Connection: close\r\n\r\n");
    while (!wifiClient.available()) {
      if (millis() - start > MAXTIMEOUT) {
        printSerial(" no reply.\n");
        wifiClient.stop();
        return false;
      }
      delay(500);
      printSerial(".");
    }
    while (wifiClient.available()) {
      printSerial(wifiClient.readStringUntil('\r'));
    }
  }

  return true;
}

void deepSleep() {
  int wakeupInterval = ((SLEEPTIME * 1000) - millis()) * 1000;
  snprintf(message, MAXMESSAGELENGTH, "sleeping for %i seconds", wakeupInterval / 1000 / 1000);
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
