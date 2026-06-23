#include "arduino_secrets.h"
#include "arduino_secrets.h"
#include "arduino_secrets.h"
#include <ArduinoMqttClient.h>
#include <WiFiNINA.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <Wire.h>
#include <Adafruit_BME280.h>

char ssid[] = SECRET_SSID;
char pass[] = SECRET_PASS;

WiFiClient wifiClient;
MqttClient mqttClient(wifiClient);
Adafruit_BME280 bme;
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 0, 60000);

unsigned long ntpEpochAtSync = 0;
unsigned long millisAtSync   = 0;

const char broker[] = "192.168.1.228";
int port = 1883;
const char topic[]  = "Temperature";
const char topic2[] = "Pressure";
const char topic3[] = "Humidity";

const long interval = 8000;
unsigned long previousMillis = 0;
int messagesSent = 0;

int tempQoS  = 0;
int pressQoS = 1;
int humQoS   = 2;

unsigned long long getEpochMs() {
  unsigned long elapsed = millis() - millisAtSync;
  return (unsigned long long)ntpEpochAtSync * 1000ULL + elapsed;
}

void setup() {
  Serial.begin(9600);
  while (!Serial);

  if (!bme.begin(0x77)) {
    Serial.println("BME280 not found");
    while (1);
  }

  while (WiFi.begin(ssid, pass) != WL_CONNECTED) delay(5000);
  Serial.println("WiFi connected");

  timeClient.begin();
  while (!timeClient.update()) {
    timeClient.forceUpdate();
    delay(500);
  }
  ntpEpochAtSync = timeClient.getEpochTime();
  millisAtSync   = millis();
  Serial.println("NTP synced");

  if (!mqttClient.connect(broker, port)) {
    Serial.print("MQTT failed: ");
    Serial.println(mqttClient.connectError());
    while (1);
  }
  Serial.println("MQTT connected");
}

void loop() {
  mqttClient.poll();

  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;

    float temperature = bme.readTemperature();
    float humidity    = bme.readHumidity();
    float pressure    = bme.readPressure() / 100.0;

    unsigned long long publishTimeMs = getEpochMs();
    unsigned long tHigh = (unsigned long)(publishTimeMs >> 32);
    unsigned long tLow  = (unsigned long)(publishTimeMs & 0xFFFFFFFF);

   mqttClient.beginMessage(topic, false, tempQoS);
    mqttClient.print(String(temperature) + "," + String(tHigh) + "," + String(tLow) + "," + String(messagesSent));
    mqttClient.endMessage();

    mqttClient.beginMessage(topic2, false, pressQoS);
    mqttClient.print(String(pressure) + "," + String(tHigh) + "," + String(tLow) + "," + String(messagesSent));
    mqttClient.endMessage();

    mqttClient.beginMessage(topic3, false, humQoS);
    mqttClient.print(String(humidity) + "," + String(tHigh) + "," + String(tLow) + "," + String(messagesSent));
    mqttClient.endMessage();

    messagesSent++;
    Serial.print("Msg #");
    Serial.println(messagesSent);
  }
}