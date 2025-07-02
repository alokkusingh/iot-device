/*********
  Alok Singh
*********/

#include <WiFi.h>
#include <PubSubClient.h>
#include <WiFiClientSecure.h>
#include "../../../../secret/secret.h"
#include "DHT.h"
#include "time.h"

#include <ArduinoJson.h>


#define DPIN 4       //Pin to connect DHT sensor (GPIO number)
#define DTYPE DHT11  //Define DHT11 or DHT22 sensor type

// WiFi and MQTT client initialization
WiFiClientSecure espClient;
PubSubClient mqttClient(espClient);

DHT dht(DPIN, DTYPE);

// Function Declarations
void connectToWiFi();
void configureNTPServer();
void configureMQTTClient();
void connectToMQTT();
void mqttCallback(char *topic, byte *payload, unsigned int length);
void senseAndPublishDHT11Telemetry();
unsigned long getEpochTime();

const char *deviceId = "esp32-general-purpose-1";
const char *statusTopic = "home/alok/status/esp32-general-purpose-1";
const char *humidityTopic = "home/alok/telemetry/humidity/esp32-general-purpose-1";
const char *temperatureTopic = "home/alok/telemetry/temperature/esp32-general-purpose-1";
const char *commandTopic = "home/alok/command/esp32-general-purpose-1";

const char *ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 0;
const int daylightOffset_sec = 0;

unsigned long previousMillis = 0;
const unsigned long interval = 60000 * 30;  // Publish interval in milliseconds (e.g., every 1 Hour)

const char *STATUS_OFFLINE = "offline";
const char *STATUS_ONLINE = "online";
const char *TAG_HUMIDITY = "humidity";
const char *TAG_TEMPERATURE = "temperature";
const char *UNIT_HUMIDITY = "%";
const char *UNIT_TEMPERATURE = "Celsius";

void setup() {
  Serial.begin(115200);

  connectToWiFi();

  configureNTPServer();

  configureMQTTClient();

  connectToMQTT();

  dht.begin();
}

void loop() {
  if (!mqttClient.connected()) {
    connectToMQTT();
  }
  mqttClient.loop();

  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;
    senseAndPublishDHT11Telemetry();
  }
}

void configureNTPServer() {
  // Init and get the time
  Serial.println("Connecting to NTP server...");
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("Failed to obtain time, lets try one more time!");
    Serial.println("Connecting to NTP server...");
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    return;
  }
  Serial.println(&timeinfo, "%A, %B %d %Y %H:%M:%S");
}

unsigned long getEpochTime() {
  time_t epochTime = time(nullptr);
  return epochTime;
}

void connectToWiFi() {
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("" + WiFi.localIP().toString());
  Serial.println("Connected to WiFi");
}

void configureMQTTClient() {
  // Set Root CA certificate
  espClient.setCACert(rootCACert);
  espClient.setCertificate(clientCert);
  espClient.setPrivateKey(clientKey);

  mqttClient.setServer(MQTT_HOST, MQTT_PORT);
  mqttClient.setKeepAlive(60);
  mqttClient.setCallback(mqttCallback);
}

void connectToMQTT() {
  while (!mqttClient.connected()) {
    Serial.printf("Connecting to MQTT Broker as %s...\n", deviceId);

    char statusPayload[128];
    getStatusPayloadJson(STATUS_OFFLINE, statusPayload, sizeof(statusPayload));

    if (mqttClient.connect(deviceId, statusTopic, 0, true, statusPayload)) {
      Serial.println("Connected to MQTT broker");

      getStatusPayloadJson(STATUS_ONLINE, statusPayload, sizeof(statusPayload));
      mqttClient.publish(statusTopic, statusPayload, false);  // Publish message upon connection
    } else {
      Serial.print("Failed to connect to MQTT broker, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" Retrying in 5 seconds.");
      delay(5000);
    }
  }
}

void mqttCallback(char *topic, byte *payload, unsigned int length) {
  Serial.print("Message received on topic: ");
  Serial.println(topic);
  Serial.print("Message: ");
  for (unsigned int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println("\n-----------------------");
}

void senseAndPublishDHT11Telemetry() {
  float tc = dht.readTemperature();      //Read temperature in C
  float tf = dht.readTemperature(true);  //Read temperature in F
  float hu = dht.readHumidity();

  Serial.print("Temp C: ");
  Serial.print(tc);
  Serial.print("\n");

  Serial.print("Temp F: ");
  Serial.print(tf);
  Serial.print("\n");

  Serial.print("Humid: ");
  Serial.print(dht.readHumidity());
  Serial.print("%\n");

  char telemetryPayloadJson[128];
  getTelemetryPayloadJson(TAG_TEMPERATURE, tc, UNIT_TEMPERATURE, telemetryPayloadJson, sizeof(telemetryPayloadJson));
  mqttClient.publish(temperatureTopic, telemetryPayloadJson);  // Publish message upon connection

  getTelemetryPayloadJson(TAG_HUMIDITY, hu, UNIT_HUMIDITY, telemetryPayloadJson, sizeof(telemetryPayloadJson));
  mqttClient.publish(humidityTopic, telemetryPayloadJson);  // Publish message upon connection
}

void getStatusPayloadJson(const char *status, char *output, size_t size) {

  JsonDocument doc;

  doc["deviceId"] = deviceId;
  doc["status"] = status;
  doc["epochTime"] = getEpochTime();
  doc["ipAddress"] = WiFi.localIP().toString();

  doc.shrinkToFit();  // optional

  size_t bytes = serializeJsonPretty(doc, output, size);
  Serial.println("Status Payload size: " + String(bytes));
}

void getTelemetryPayloadJson(const char *tag, double value, const char *unit, char *output, size_t size) {

  JsonDocument doc;

  doc["deviceId"] = deviceId;
  doc["epochTime"] = getEpochTime();
  doc[tag] = value;
  doc["unit"] = unit;

  doc.shrinkToFit();  // optional

  size_t bytes = serializeJsonPretty(doc, output, size);
  Serial.println("Telemetry Payload size: " + String(bytes));
}
