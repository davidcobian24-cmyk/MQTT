#include "arduino_secrets.h"
#include "arduino_secrets.h"
#include "thingProperties.h"
#include <ArduinoMqttClient.h>
#include <WiFiNINA.h>
#include <WiFiUdp.h>
#include <NTPClient.h>

char ssid[] = SECRET_SSID;
char pass[] = SECRET_PASS;

WiFiClient wifiClient;
MqttClient mqttClient(wifiClient);
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 0, 60000);

unsigned long ntpEpochAtSync = 0;
unsigned long millisAtSync   = 0;

const char broker[] = "192.168.1.228";
int port = 1883;
const char topic[]  = "Temperature";
const char topic2[] = "Pressure";
const char topic3[] = "Humidity";

int tempQoS  = 0;
int pressQoS = 1;
int humQoS   = 2;

int tempReceived  = 0, tempMissed  = 0, tempLastMsg  = -1;
int pressReceived = 0, pressMissed = 0, pressLastMsg = -1;
int humReceived   = 0, humMissed   = 0, humLastMsg   = -1;

long tempLatency  = 0;
long pressLatency = 0;
long humLatency   = 0;

unsigned long long getEpochMs() {
  unsigned long elapsed = millis() - millisAtSync;
  return (unsigned long long)ntpEpochAtSync * 1000ULL + elapsed;
}

void setup() {
  Serial.begin(9600);
  delay(2000);

  initProperties();
  ArduinoCloud.begin(ArduinoIoTPreferredConnection);

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

  mqttClient.onMessage(onMqttMessage);
  mqttClient.subscribe(topic,  tempQoS);
  mqttClient.subscribe(topic2, pressQoS);
  mqttClient.subscribe(topic3, humQoS);
  Serial.println("Subscribed");
}

void loop() {
  ArduinoCloud.update();
  mqttClient.poll();

  Qos0 = tempLatency;
  Qos1 = pressLatency;
  Qos2 = humLatency;
}

void handleMessage(String rxTopic, String payload, String unit, int qos,
                   int &received, int &missed, int &lastMsg) {
  int c1 = payload.indexOf(',');
  int c2 = payload.indexOf(',', c1 + 1);
  int c3 = payload.lastIndexOf(',');
  if (c1 == -1 || c2 == -1 || c3 == -1 || c1 == c2 || c2 == c3) {
    Serial.println("bad payload: " + payload);
    return;
  }

  float value         = payload.substring(0, c1).toFloat();
  unsigned long tHigh = strtoul(payload.substring(c1 + 1, c2).c_str(), NULL, 10);
  unsigned long tLow  = strtoul(payload.substring(c2 + 1, c3).c_str(), NULL, 10);
  int msgNum          = payload.substring(c3 + 1).toInt();

  unsigned long long sentMs    = ((unsigned long long)tHigh << 32) | tLow;
  unsigned long long receiveMs = getEpochMs();
  long latency = (long)(receiveMs - sentMs);
  if (latency < 0) latency = 0;

  if      (rxTopic == topic)  tempLatency  = latency;
  else if (rxTopic == topic2) pressLatency = latency;
  else if (rxTopic == topic3) humLatency   = latency;

  received++;
  if (lastMsg != -1 && msgNum != lastMsg + 1) {
    missed += msgNum - lastMsg - 1;
    Serial.println("dropped messages!");
  }
  lastMsg = msgNum;

  Serial.print(rxTopic);
  Serial.print(" (QoS "); Serial.print(qos); Serial.print("): ");
  Serial.print(value); Serial.println(" " + unit);
  Serial.print("  Latency: "); Serial.print(latency); Serial.println(" ms");
  Serial.print("  Msg #"); Serial.print(msgNum);
  Serial.print(" | Received: "); Serial.print(received);
  Serial.print(" | Missed: "); Serial.println(missed);
  Serial.println();
}

void onMqttMessage(int messageSize) {
  String rxTopic = mqttClient.messageTopic();
  String payload = "";
  while (mqttClient.available()) payload += (char)mqttClient.read();

  if      (rxTopic == topic)  handleMessage(rxTopic, payload, "C",   tempQoS,  tempReceived,  tempMissed,  tempLastMsg);
  else if (rxTopic == topic2) handleMessage(rxTopic, payload, "hPa", pressQoS, pressReceived, pressMissed, pressLastMsg);
  else if (rxTopic == topic3) handleMessage(rxTopic, payload, "%",   humQoS,   humReceived,   humMissed,   humLastMsg);
}

void onQos0LatencyChange() {}
void onQos1LatencyChange() {}
void onQos2LatencyChange() {}
