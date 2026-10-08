#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "lab_config.h"

WiFiClient espClient;
PubSubClient client(espClient);
String clientID, rssiTopic, ledTopic, statusTopic;
constexpr uint8_t LED_PINS[] = {25, 26, 27};
constexpr const char *LED_KEYS[] = {"led_red", "led_green", "led_blue"};
uint32_t lastWifi = 0, lastMqtt = 0, lastPublish = 0;
bool statusPending = true;

void callback(char *topic, byte *payload, unsigned int length) {
  if (strcmp(topic, ledTopic.c_str()) != 0 || length > 192) return;
  JsonDocument doc;
  if (deserializeJson(doc, payload, length)) return;
  for (unsigned i = 0; i < 3; ++i) {
    if (!doc[LED_KEYS[i]].is<const char *>()) return;
    const char *v = doc[LED_KEYS[i]];
    if (strcmp(v, "0") != 0 && strcmp(v, "1") != 0) return;
  }
  for (unsigned i = 0; i < 3; ++i)
    digitalWrite(LED_PINS[i], strcmp(doc[LED_KEYS[i]], "1") == 0 ? HIGH : LOW);
  statusPending = true;
  Serial.println("Week 8 JSON received and all three LED outputs updated");
}
void setup() {
  Serial.begin(115200);
  for (auto pin : LED_PINS) { pinMode(pin, OUTPUT); digitalWrite(pin, LOW); }
  WiFi.mode(WIFI_STA);
  WiFi.begin(Config::WIFI_SSID, Config::WIFI_PASSWORD);
  clientID = String(Config::GROUP_ID) + "-demo-" + WiFi.macAddress();
  const String root = String("iot2026/") + Config::GROUP_ID + "/demo";
  rssiTopic = root + "/wifiMeta/rssi";
  ledTopic = root + "/msgIn/led_group";
  statusTopic = root + "/status";
  client.setServer(Config::MQTT_HOST, Config::MQTT_PORT);
  client.setCallback(callback);
  client.setSocketTimeout(1);
  client.setBufferSize(768);
  Serial.printf("\nWeek 8 demo: %s\nRSSI: %s\nCommand: %s\n", clientID.c_str(),
                rssiTopic.c_str(), ledTopic.c_str());
  lastMqtt = millis() - 3000;
}
void loop() {
  const uint32_t now = millis();
  if (WiFi.status() != WL_CONNECTED) {
    if (uint32_t(now - lastWifi) >= 10000) { lastWifi = now; WiFi.reconnect(); }
  } else if (!client.connected()) {
    if (uint32_t(now - lastMqtt) >= 3000) {
      lastMqtt = now;
      if (client.connect(clientID.c_str(), Config::MQTT_USER, Config::MQTT_PASSWORD)) {
        if (!client.subscribe(ledTopic.c_str())) client.disconnect();
        else { Serial.println("MQTT connected and subscribed"); statusPending = true; }
      } else Serial.printf("MQTT failed rc=%d\n", client.state());
    }
  } else {
    client.loop();
    if (statusPending || uint32_t(now - lastPublish) >= 2000) {
      lastPublish = now;
      char rssi[16];
      snprintf(rssi, sizeof(rssi), "%d", WiFi.RSSI());
      client.publish(rssiTopic.c_str(), rssi);
      JsonDocument doc;
      doc["group"] = Config::GROUP_ID;
      doc["device"] = clientID;
      doc["uptime_ms"] = now;
      for (unsigned i = 0; i < 3; ++i) doc[LED_KEYS[i]] = digitalRead(LED_PINS[i]) ? "1" : "0";
      char json[512];
      serializeJson(doc, json, sizeof(json));
      statusPending = !client.publish(statusTopic.c_str(), json);
      Serial.printf("RSSI=%s dBm; LED status=%s\n", rssi, json);
    }
  }
  delay(1);
}
