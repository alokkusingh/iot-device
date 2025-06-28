/*********
  Alok Singh
*********/

#include <WiFi.h>
#include <PubSubClient.h>
#include <WiFiClientSecure.h>

#include "../../../../secret/secret.h"

#include "DHT.h"

#define DPIN 4       //Pin to connect DHT sensor (GPIO number)
#define DTYPE DHT11  //Define DHT11 or DHT22 sensor type

time_t now;
unsigned long lastMillis = 0;

const char *clientId = "esp32-general-purpose-1";
const char *statusTopic = "home/alok/status/esp32-general-purpose-1";
const char *humidityTopic = "home/alok/telemetry/humidity/esp32-general-purpose-1";
const char *temperatureTopic = "home/alok/telemetry/temperature/esp32-general-purpose-1";
const char *commandTopic = "home/alok/command/esp32-general-purpose-1";

// WiFi and MQTT client initialization
WiFiClientSecure esp_client;
PubSubClient mqtt_client(esp_client);

DHT dht(DPIN, DTYPE);

// Function Declarations
void connectToWiFi();

void connectToMQTT();

void mqttCallback(char *topic, byte *payload, unsigned int length);


void setup() {
  Serial.begin(115200);
  connectToWiFi();

  // Set Root CA certificate
  esp_client.setCACert(rootCACert);
  esp_client.setCertificate(clientCert);
  esp_client.setPrivateKey(clientKey);

  mqtt_client.setServer(MQTT_HOST, MQTT_PORT);
  mqtt_client.setKeepAlive(60);
  mqtt_client.setCallback(mqttCallback);

  connectToMQTT();

  dht.begin();
}

void loop() {
  if (!mqtt_client.connected()) {
    connectToMQTT();
  }
  mqtt_client.loop();

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

  String str = "{\"deviceId\":\"";
  String temperaturePayload = str + clientId + "\", \"temperature\":" + tc + ", \"unit\":\"Celsius\"}";
  mqtt_client.publish(temperatureTopic, temperaturePayload.c_str());  // Publish message upon connection

  String humidityPayload = str + clientId + "\", \"humidity\":" + hu + ", \"unit\":\"%\"}";
  mqtt_client.publish(humidityTopic, humidityPayload.c_str());  // Publish message upon connection

  delay(60000 * 5);
}

void connectToWiFi() {
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("" + WiFi.localIP().toString());
  Serial.println("\nConnected to WiFi");
}

void connectToMQTT() {
  while (!mqtt_client.connected()) {
    Serial.printf("Connecting to MQTT Broker as %s...\n", clientId);
    
    String str = "{\"deviceId\":\"";
    String willPayload = str + clientId + "\", \"status\":\"offline\", \"lastSeen\":\"\", \"ipAddress\":\"" + WiFi.localIP().toString() + "\"" + "}";
    
    if (mqtt_client.connect(clientId, statusTopic, 0, true, willPayload.c_str())) {
      Serial.println("Connected to MQTT broker");
      mqtt_client.subscribe(commandTopic);

      String connectedStatusPayload = str + clientId + "\", \"status\":\"online\", \"lastSeen\":\"\", \"ipAddress\":\"" + WiFi.localIP().toString() + "\"" + "}";

      mqtt_client.publish(statusTopic, connectedStatusPayload.c_str(), false);  // Publish message upon connection
    } else {
      Serial.print("Failed to connect to MQTT broker, rc=");
      Serial.print(mqtt_client.state());
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
