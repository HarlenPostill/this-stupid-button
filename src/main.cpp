#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <esp_system.h>
#include "lab_config.h"
#include "alarm_logic.h"

// Week 8 slides 32-33: same transport, MQTT library, callback and JSON parser.
// Networking has its own task because even a single PubSubClient connect() can
// block. Queues keep sensing, local acknowledgement and timing independent.
namespace {
WiFiClient espClient;
PubSubClient client(espClient);
String baseTopic, temperatureTopic, statusTopic, commandTopic, availabilityTopic;
String clientID;
char bootID[17];
AlarmLogic alarmLogic;
float temperature = NAN;
uint32_t sensorMv = 0;
const char *ackSource = "none";
uint32_t lastSample = 0;
uint32_t lastSnapshot = 0;
unsigned previousHz = 0;
bool rawButton = HIGH, stableButton = HIGH;
uint32_t buttonChangedAt = 0;

struct Snapshot {
  float temperature;
  uint32_t mv, uptime, revision;
  AlarmState state;
  bool acknowledged;
  char alarmID[32];
  char source[8];
};
struct Command { char alarmID[32]; };
QueueHandle_t snapshotQueue, commandQueue;

void currentAlarmID(char *out, size_t size) {
  snprintf(out, size, "%s-%lu", bootID, static_cast<unsigned long>(alarmLogic.episode));
}

void callback(char *topic, byte *payload, unsigned int length) {
  if (strcmp(topic, commandTopic.c_str()) != 0 || length == 0 || length > 192) return;
  JsonDocument doc;
  // MQTT payload is length-delimited, not necessarily null-terminated.
  if (deserializeJson(doc, payload, length) || !doc.is<JsonObject>() ||
      !doc["command"].is<const char *>() || !doc["alarm_id"].is<const char *>()) {
    Serial.println("Command rejected: expected command and alarm_id strings");
    return;
  }
  const char *action = doc["command"];
  const char *id = doc["alarm_id"];
  if (strcmp(action, Config::ACK_COMMAND) != 0 || strlen(id) >= sizeof(Command::alarmID)) {
    Serial.println("Command rejected: unknown command or oversized alarm_id");
    return;
  }
  Command cmd{};
  strlcpy(cmd.alarmID, id, sizeof(cmd.alarmID));
  if (xQueueSend(commandQueue, &cmd, 0) != pdTRUE)
    Serial.println("Command queue full; retry using current status");
}

bool publishSnapshot(const Snapshot &s) {
  char value[24];
  if (isfinite(s.temperature)) snprintf(value, sizeof(value), "%.2f", s.temperature);
  else strlcpy(value, "null", sizeof(value));
  const bool sentTemperature = client.publish(temperatureTopic.c_str(), value, true);
  JsonDocument doc;
  doc["group"] = Config::GROUP_ID;
  doc["device"] = clientID;
  doc["state"] = stateName(s.state);
  doc["acknowledged"] = s.acknowledged;
  doc["ack_source"] = s.source;
  doc["alarm_id"] = s.alarmID;
  doc["sensor_ok"] = isfinite(s.temperature);
  if (isfinite(s.temperature)) doc["temperature_c"] = serialized(value);
  else doc["temperature_c"] = nullptr;
  doc["threshold_c"] = Config::TRIGGER_C;
  doc["reset_c"] = Config::TRIGGER_C - Config::HYSTERESIS_C;
  doc["adc_mv"] = s.mv;
  doc["uptime_ms"] = s.uptime;
  doc["rssi_dbm"] = WiFi.RSSI();
  char json[768];
  if (measureJson(doc) >= sizeof(json)) return false;
  serializeJson(doc, json, sizeof(json));
  return client.publish(statusTopic.c_str(), json, true) && sentTemperature;
}

void mqttTask(void *) {
  // Only this task touches client or espClient: PubSubClient isn't thread-safe.
  client.setServer(Config::MQTT_HOST, Config::MQTT_PORT);
  client.setCallback(callback);
  client.setKeepAlive(15);
  client.setSocketTimeout(1);
  if (!client.setBufferSize(1024)) {
    Serial.println("MQTT buffer allocation failed; local alarm still runs");
    vTaskDelete(nullptr);
    return;
  }
  WiFi.begin(Config::WIFI_SSID, Config::WIFI_PASSWORD);
  uint32_t wifiRetry = millis(), mqttRetry = millis() - 3000;
  uint32_t lastPublish = 0, publishedRevision = UINT32_MAX;
  Snapshot latest{};
  bool haveSnapshot = false, forcePublish = true;
  for (;;) {
    if (xQueueReceive(snapshotQueue, &latest, 0) == pdTRUE) haveSnapshot = true;
    const uint32_t now = millis();
    if (WiFi.status() != WL_CONNECTED) {
      if (client.connected()) client.disconnect();
      if (uint32_t(now - wifiRetry) >= 10000) {
        wifiRetry = now;
        WiFi.reconnect();
        Serial.println("Retrying Wi-Fi; check 2.4 GHz SSID/password");
      }
      forcePublish = true;
    } else if (!client.connected()) {
      if (uint32_t(now - mqttRetry) >= 3000) {
        mqttRetry = now;
        if (client.connect(clientID.c_str(), Config::MQTT_USER, Config::MQTT_PASSWORD,
                           availabilityTopic.c_str(), 1, true, "offline")) {
          // Like the lecture, resubscribe on EVERY connection.
          if (!client.subscribe(commandTopic.c_str(), 0)) {
            client.disconnect();
          } else {
            client.publish(availabilityTopic.c_str(), "online", true);
            Serial.printf("MQTT connected: %s | IP %s\n", Config::MQTT_HOST,
                          WiFi.localIP().toString().c_str());
            forcePublish = true;
          }
        } else Serial.printf("MQTT connection failed, rc=%d\n", client.state());
      }
    } else {
      client.loop(); // Services incoming commands and MQTT keepalive frequently.
      if (haveSnapshot && (forcePublish || latest.revision != publishedRevision ||
                           uint32_t(now - lastPublish) >= 1000)) {
        if (publishSnapshot(latest)) {
          publishedRevision = latest.revision;
          forcePublish = false;
        }
        lastPublish = now;
        // Avoid a tight retry loop if a publish fails.
        if (!client.connected()) forcePublish = true;
      }
    }
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

void acknowledge(const char *source) {
  if (alarmLogic.acknowledge()) {
    ackSource = source;
    Serial.printf("Alarm acknowledged locally/remotely: %s\n", source);
  }
}

void updateButton(uint32_t now) {
  const bool reading = digitalRead(Config::BUTTON_PIN);
  if (reading != rawButton) { rawButton = reading; buttonChangedAt = now; }
  if (rawButton != stableButton && uint32_t(now - buttonChangedAt) >= 30) {
    stableButton = rawButton;
    if (stableButton == LOW) acknowledge("local"); // Debounced press edge only.
  }
}

void sendSnapshot(uint32_t now) {
  Snapshot s{};
  s.temperature = temperature;
  s.mv = sensorMv;
  s.uptime = now;
  s.revision = alarmLogic.revision;
  s.state = alarmLogic.state;
  s.acknowledged = alarmLogic.acknowledged;
  currentAlarmID(s.alarmID, sizeof(s.alarmID));
  strlcpy(s.source, ackSource, sizeof(s.source));
  // A one-element mailbox always holds current data, not an offline backlog.
  xQueueOverwrite(snapshotQueue, &s);
}
}

void setup() {
  Serial.begin(115200);
  pinMode(Config::BUTTON_PIN, INPUT_PULLUP);
  analogReadResolution(12);
  analogSetPinAttenuation(Config::THERMISTOR_PIN, ADC_11db);
  ledcSetup(0, 1800, 8); // Arduino-ESP32 2.x, pinned by platformio.ini.
  ledcAttachPin(Config::BUZZER_PIN, 0);
  ledcWrite(0, 0);
  WiFi.mode(WIFI_STA);
  snprintf(bootID, sizeof(bootID), "%08lx%08lx",
           static_cast<unsigned long>(esp_random()), static_cast<unsigned long>(esp_random()));
  String mac = WiFi.macAddress();
  mac.replace(":", "");
  clientID = String(Config::GROUP_ID) + "-fire-" + mac;
  baseTopic = String("iot2026/") + Config::GROUP_ID + "/fire";
  temperatureTopic = baseTopic + "/temperature";
  statusTopic = baseTopic + "/status";
  commandTopic = baseTopic + "/command";
  availabilityTopic = baseTopic + "/availability";
  Serial.printf("\nWeek 9 fire alarm | %s\nSubscribe in Explorer: iot2026/%s/#\n",
                clientID.c_str(), Config::GROUP_ID);
  Serial.printf("Trigger > %.1f C; reset <= %.1f C; escalation %lu ms\n",
                Config::TRIGGER_C, Config::TRIGGER_C - Config::HYSTERESIS_C, Config::ESCALATION_MS);
  snapshotQueue = xQueueCreate(1, sizeof(Snapshot));
  commandQueue = xQueueCreate(8, sizeof(Command));
  if (!snapshotQueue || !commandQueue) {
    Serial.println("Queue allocation failed");
    abort();
  }
  lastSample = millis() - 100;
  if (xTaskCreate(mqttTask, "mqtt", 8192, nullptr, 1, nullptr) != pdPASS)
    Serial.println("MQTT task unavailable; local alarm still runs");
}

void loop() {
  const uint32_t now = millis();
  const uint32_t oldRevision = alarmLogic.revision;
  if (uint32_t(now - lastSample) >= 100) {
    lastSample = now;
    uint32_t sum = 0;
    for (unsigned i = 0; i < 8; ++i) sum += analogReadMilliVolts(Config::THERMISTOR_PIN);
    sensorMv = sum / 8;
    temperature = thermistorC(sensorMv, Config::SUPPLY_MV, Config::SERIES_OHMS,
                             Config::NTC_NOMINAL_OHMS, Config::NTC_BETA, Config::TEMP_OFFSET_C);
    alarmLogic.sample(temperature, now, Config::TRIGGER_C, Config::HYSTERESIS_C);
    if (!alarmLogic.acknowledged) ackSource = "none";
  }
  updateButton(now);
  Command cmd{};
  // Bounded processing prevents a flood of commands from starving local logic.
  for (unsigned i = 0; i < 8 && xQueueReceive(commandQueue, &cmd, 0) == pdTRUE; ++i) {
    char activeID[32];
    currentAlarmID(activeID, sizeof(activeID));
    if (strcmp(cmd.alarmID, activeID) == 0) acknowledge("remote");
    else Serial.println("Command rejected: stale/wrong alarm_id; copy current status ID");
  }
  alarmLogic.tick(now, Config::ESCALATION_MS);
  const unsigned hz = alarmLogic.buzzerHz(now);
  if (hz != previousHz) {
    if (hz) ledcWriteTone(0, hz);
    else ledcWrite(0, 0);
    previousHz = hz;
  }
  if (alarmLogic.revision != oldRevision || uint32_t(now - lastSnapshot) >= 100) {
    sendSnapshot(now);
    lastSnapshot = now;
  }
  static uint32_t lastLog = 0;
  if (alarmLogic.revision != oldRevision || uint32_t(now - lastLog) >= 1000) {
    Serial.printf("T=%.2f C ADC=%lu mV state=%s acknowledged=%s\n", temperature,
                  static_cast<unsigned long>(sensorMv), stateName(alarmLogic.state),
                  alarmLogic.acknowledged ? "true" : "false");
    lastLog = now;
  }
  delay(1); // Yield to the scheduler; never delay for a beep or escalation timer.
}
