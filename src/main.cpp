#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <Freenove_WS2812_Lib_for_ESP32.h>
#include <driver/i2s.h>
#include <esp_err.h>
#include <esp_system.h>
#include "audio_level.h"
#include "noise_config.h"
#include "noise_logic.h"
#include "local_page.h"

namespace {
constexpr i2s_port_t MIC_PORT = I2S_NUM_0;
Freenove_ESP32_WS2812 strip(Config::LED_COUNT, Config::LED_PIN, 0, TYPE_GRB);
Preferences preferences;
Noise::Settings settings;
Noise::State state = Noise::State::Fault;
AudioLevel::Window window;
int32_t samples[256];
bool micReady = false, storageReady = false, ledReady = false;
uint32_t sequence = 0, revision = 0;
float currentDbfs = NAN;
bool currentClipped = false;

struct Snapshot {
  float dbfs, spl;
  Noise::Settings settings;
  Noise::State state;
  uint32_t sampledAt, sequence, revision;
  bool sensorOk, clipped;
};
QueueHandle_t snapshots, commands;
char deviceID[48], bootID[17];
String telemetryTopic, statusTopic, availabilityTopic;

// These objects and variables are owned exclusively by networkTask.
WiFiClient transport;
PubSubClient mqtt(transport);
WebServer server(80);
Snapshot latest{};
bool haveSnapshot = false;

bool startMicrophone() {
  i2s_config_t config{};
  config.mode = static_cast<i2s_mode_t>(I2S_MODE_MASTER | I2S_MODE_RX);
  config.sample_rate = Config::SAMPLE_RATE;
  config.bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT;
  config.channel_format = I2S_CHANNEL_FMT_ONLY_LEFT; // L/R tied to GND.
  config.communication_format = I2S_COMM_FORMAT_STAND_I2S;
  config.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
  config.dma_buf_count = 8;
  config.dma_buf_len = 256;
  config.bits_per_chan = I2S_BITS_PER_CHAN_32BIT;
  esp_err_t error = i2s_driver_install(MIC_PORT, &config, 0, nullptr);
  if (error != ESP_OK) {
    Serial.printf("I2S install failed: %s\n", esp_err_to_name(error));
    return false;
  }
  i2s_pin_config_t pins{};
  pins.mck_io_num = I2S_PIN_NO_CHANGE;
  pins.bck_io_num = Config::MIC_SCK;
  pins.ws_io_num = Config::MIC_WS;
  pins.data_out_num = I2S_PIN_NO_CHANGE;
  pins.data_in_num = Config::MIC_SD;
  error = i2s_set_pin(MIC_PORT, &pins);
  if (error != ESP_OK) {
    Serial.printf("I2S pins failed: %s\n", esp_err_to_name(error));
    i2s_driver_uninstall(MIC_PORT);
    return false;
  }
  // SD tri-states outside its 24 data bits. External 100k is optional.
  pinMode(Config::MIC_SD, INPUT_PULLDOWN);
  const uint32_t started = millis();
  while (uint32_t(millis() - started) < 300) {
    size_t bytesRead = 0;
    i2s_read(MIC_PORT, samples, sizeof(samples), &bytesRead, pdMS_TO_TICKS(50));
  }
  return true;
}

void showState(Noise::State next) {
  if (!ledReady) return;
  static int displayed = -1;
  static bool displayedRedOn = false;
  static uint32_t redStartedAt = 0;
  const uint32_t now = millis();
  const bool stateChanged = displayed != static_cast<int>(next);
  if (stateChanged && next == Noise::State::High) redStartedAt = now;
  const bool redOn = next == Noise::State::High &&
      uint32_t(now - redStartedAt) % (2 * Config::RED_FLASH_HALF_PERIOD_MS) <
          Config::RED_FLASH_HALF_PERIOD_MS;
  if (!stateChanged && redOn == displayedRedOn) return;
  displayed = static_cast<int>(next);
  displayedRedOn = redOn;
  uint8_t r = 0, g = 0, b = 0;
  switch (next) {
    case Noise::State::Safe: g = 255; break;
    case Noise::State::Warning: r = 255; g = 100; break;
    case Noise::State::High: if (redOn) r = 255; break;
    case Noise::State::Fault: r = 140; b = 255; break; // Fault must never look safe.
  }
  for (uint8_t i = 0; i < Config::LED_COUNT; ++i) strip.setLedColorData(i, r, g, b);
  // Freenove v1.0.5 declares esp_err_t but forwards rmtWrite's bool:
  // 1 = success, 0 = failure. Comparing with ESP_OK reverses that meaning.
  if (!strip.show()) Serial.println("LED RMT write failed");
}

void emitSnapshot() {
  const float spl = isfinite(currentDbfs)
      ? currentDbfs + 120.0f + settings.offsetDb - Config::SPL_REDUCTION_DB : NAN;
  const Noise::State next = Noise::classify(spl, state, settings, Config::HYSTERESIS_DB);
  if (next != state) ++revision;
  state = next;
  showState(state);
  Snapshot s{};
  s.dbfs = currentDbfs;
  s.spl = spl;
  s.settings = settings;
  s.state = state;
  s.sampledAt = millis();
  s.sequence = ++sequence;
  s.revision = revision;
  s.sensorOk = isfinite(spl);
  s.clipped = currentClipped;
  xQueueOverwrite(snapshots, &s); // Current reading only, never an offline backlog.
  if (s.sensorOk) {
    Serial.printf("Noise %.1f dB SPL estimate | %.1f dBFS | %s%s\n",
                  spl, currentDbfs, Noise::name(state), currentClipped ? " | CLIPPING" : "");
  } else Serial.println("Microphone fault/no varying audio: check wiring (purple LEDs)");
}

void loadSettings() {
  settings.warningDb = Config::WARNING_DB;
  settings.limitDb = Config::LIMIT_DB;
  storageReady = preferences.begin("noise-monitor", false);
  if (!storageReady) {
    Serial.println("NVS unavailable: defaults active; configuration writes disabled");
    return;
  }
  // One blob prevents partially updated threshold pairs after a power cut.
  if (preferences.getBytesLength("settings") == sizeof(settings)) {
    Noise::Settings saved{};
    preferences.getBytes("settings", &saved, sizeof(saved));
    if (Noise::valid(saved, Config::HYSTERESIS_DB)) settings = saved;
  }
}

void applyCommands() {
  Noise::Settings requested{};
  if (xQueueReceive(commands, &requested, 0) != pdTRUE) return;
  if (!Noise::valid(requested, Config::HYSTERESIS_DB)) return;
  if (preferences.putBytes("settings", &requested, sizeof(requested)) != sizeof(requested)) {
    Serial.println("Configuration save failed; previous settings remain active");
    return;
  }
  settings = requested;
  ++revision;
  // Reclassify on the next complete audio window using the new offset/limits.
  state = Noise::State::Fault;
  window = AudioLevel::Window{};
  Serial.printf("Saved: orange %.1f dB, red %.1f dB, offset %.1f dB\n",
                settings.warningDb, settings.limitDb, settings.offsetDb);
}

void fillDocument(JsonDocument &doc) {
  const uint32_t age = haveSnapshot ? uint32_t(millis() - latest.sampledAt) : UINT32_MAX;
  const bool ok = haveSnapshot && latest.sensorOk && age <= Config::STALE_MS;
  doc["group"] = Config::GROUP_ID;
  doc["device"] = deviceID;
  doc["location"] = Config::LOCATION;
  doc["firmware"] = Config::FIRMWARE_VERSION;
  doc["boot_id"] = bootID;
  doc["sensor_ok"] = ok;
  doc["state"] = Noise::name(ok ? latest.state : Noise::State::Fault);
  doc["state_code"] = static_cast<uint8_t>(ok ? latest.state : Noise::State::Fault);
  if (ok) {
    doc["estimated_spl_db"] = latest.spl;
    doc["dbfs"] = latest.dbfs;
  } // No NaN or invented 0 dB readings on sensor failure.
  doc["clipped"] = haveSnapshot && latest.clipped;
  doc["calibrated"] = latest.settings.calibrated;
  doc["warning_db"] = latest.settings.warningDb;
  doc["limit_db"] = latest.settings.limitDb;
  doc["offset_db"] = latest.settings.offsetDb;
  doc["hysteresis_db"] = Config::HYSTERESIS_DB;
  doc["sequence"] = latest.sequence;
  doc["sample_age_ms"] = age;
  doc["uptime_ms"] = millis();
  doc["window_ms"] = 500;
  doc["sample_rate_hz"] = Config::SAMPLE_RATE;
  doc["rssi_dbm"] = WiFi.status() == WL_CONNECTED ? WiFi.RSSI() : 0;
}

void configureWeb() {
  server.on("/", HTTP_GET, [] { server.send_P(200, "text/html", LOCAL_PAGE); });
  server.on("/api/status", HTTP_GET, [] {
    JsonDocument doc;
    fillDocument(doc);
    doc["mqtt_connected"] = mqtt.connected();
    doc["storage_ready"] = storageReady;
    String json;
    serializeJson(doc, json);
    server.sendHeader("Cache-Control", "no-store");
    server.send(200, "application/json", json);
  });
  server.on("/api/config", HTTP_POST, [] {
    if (!storageReady) {
      server.send(503, "application/json", "{\"message\":\"Board settings storage unavailable.\"}");
      return;
    }
    if (server.arg("plain").length() > 256) {
      server.send(413, "application/json", "{\"message\":\"Request too large.\"}");
      return;
    }
    JsonDocument doc;
    if (deserializeJson(doc, server.arg("plain")) || !doc.is<JsonObject>() ||
        !doc["warning_db"].is<float>() || !doc["limit_db"].is<float>() ||
        !doc["offset_db"].is<float>() || !doc["calibrated"].is<bool>()) {
      server.send(400, "application/json", "{\"message\":\"Expected numeric warning_db, limit_db, offset_db and boolean calibrated.\"}");
      return;
    }
    Noise::Settings requested{};
    requested.warningDb = doc["warning_db"];
    requested.limitDb = doc["limit_db"];
    requested.offsetDb = doc["offset_db"];
    requested.calibrated = doc["calibrated"];
    if (!Noise::valid(requested, Config::HYSTERESIS_DB)) {
      server.send(400, "application/json", "{\"message\":\"Use 30..120 dB thresholds, red > orange + 2 dB, and offset -30..30 dB.\"}");
    } else if (xQueueSend(commands, &requested, 0) != pdTRUE) {
      server.send(503, "application/json", "{\"message\":\"Another update is pending. Retry shortly.\"}");
    } else {
      server.send(202, "application/json", "{\"message\":\"Update queued; waiting for saved settings.\"}");
    }
  });
  server.onNotFound([] { server.send(404, "application/json", "{\"message\":\"Not found.\"}"); });
  server.begin();
}

bool publishSnapshot() {
  JsonDocument doc;
  fillDocument(doc);
  char json[1024];
  if (measureJson(doc) >= sizeof(json)) return false;
  serializeJson(doc, json, sizeof(json));
  // Telegraf and board 2 consume live telemetry. Status is a retained viewer snapshot.
  const bool telemetrySent = mqtt.publish(telemetryTopic.c_str(), json, false);
  const bool statusSent = mqtt.publish(statusTopic.c_str(), json, true);
  return telemetrySent && statusSent;
}

void networkTask(void *) {
  configureWeb();
  mqtt.setServer(Config::MQTT_HOST, Config::MQTT_PORT);
  mqtt.setKeepAlive(15);
  mqtt.setSocketTimeout(1);
  transport.setTimeout(1);
  const bool mqttReady = mqtt.setBufferSize(1536);
  if (!mqttReady) Serial.println("MQTT buffer allocation failed; local sensing/web still run");
  const bool configured = Config::WIFI_SSID[0] && Config::GROUP_ID[0] &&
      !strstr(Config::WIFI_SSID, "YOUR_") && !strstr(Config::GROUP_ID, "YOUR_");
  if (configured) WiFi.begin(Config::WIFI_SSID, Config::WIFI_PASSWORD);
  else Serial.println("Wi-Fi disabled: set LAB_WIFI_SSID/PASSWORD/GROUP_ID in include/secrets.h");
  uint32_t wifiRetry = millis(), mqttRetry = millis() - 3000, lastPublish = 0;
  uint32_t publishedRevision = UINT32_MAX, publishedSequence = UINT32_MAX;
  bool wasWifiConnected = false, forcePublish = true;
  for (;;) {
    if (xQueueReceive(snapshots, &latest, 0) == pdTRUE) haveSnapshot = true;
    server.handleClient(); // Same LAN; independent of broker/cloud availability.
    const uint32_t now = millis();
    if (WiFi.status() != WL_CONNECTED) {
      wasWifiConnected = false;
      if (mqtt.connected()) mqtt.disconnect();
      if (configured && uint32_t(now - wifiRetry) >= 10000) {
        wifiRetry = now;
        WiFi.reconnect();
        Serial.println("Retrying Wi-Fi; microphone/LEDs continue locally");
      }
      forcePublish = true;
    } else {
      if (!wasWifiConnected) {
        Serial.printf("Local page: http://%s/\n", WiFi.localIP().toString().c_str());
        wasWifiConnected = true;
      }
      if (mqttReady && !mqtt.connected()) {
        if (uint32_t(now - mqttRetry) >= 3000) {
          mqttRetry = now;
          if (mqtt.connect(deviceID, Config::MQTT_USER, Config::MQTT_PASSWORD,
                           availabilityTopic.c_str(), 1, true, "offline")) {
            mqtt.publish(availabilityTopic.c_str(), "online", true);
            forcePublish = true;
            Serial.println("MQTT connected; telemetry -> Telegraf -> InfluxDB");
          } else Serial.printf("MQTT connect failed, rc=%d\n", mqtt.state());
        }
      } else if (mqttReady) {
        mqtt.loop();
        // A slow reconnect may have taken seconds: refresh before publishing.
        if (xQueueReceive(snapshots, &latest, 0) == pdTRUE) haveSnapshot = true;
        if (haveSnapshot && (forcePublish || latest.revision != publishedRevision ||
            (latest.sequence != publishedSequence && uint32_t(now - lastPublish) >= Config::PUBLISH_MS))) {
          if (publishSnapshot()) {
            publishedRevision = latest.revision;
            publishedSequence = latest.sequence;
            forcePublish = false;
          }
          lastPublish = millis();
        }
      }
    }
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}
}

void setup() {
  Serial.begin(115200);
  loadSettings();
  ledReady = strip.begin();
  if (ledReady) strip.setBrightness(Config::LED_BRIGHTNESS);
  else Serial.println("LED RMT setup failed; microphone/network remain available");
  showState(Noise::State::Fault);
  snapshots = xQueueCreate(1, sizeof(Snapshot));
  commands = xQueueCreate(1, sizeof(Noise::Settings));
  if (!snapshots || !commands) {
    Serial.println("Queue allocation failed");
    abort();
  }
  WiFi.mode(WIFI_STA);
  String mac = WiFi.macAddress();
  mac.replace(":", "");
  // Group lives in the topic, MAC keeps the MQTT client ID short and unique.
  snprintf(deviceID, sizeof(deviceID), "noise-mic-%s", mac.c_str());
  snprintf(bootID, sizeof(bootID), "%08lx%08lx",
           static_cast<unsigned long>(esp_random()), static_cast<unsigned long>(esp_random()));
  const String base = String("iot2026/") + Config::GROUP_ID + "/noise/" + deviceID;
  telemetryTopic = base + "/telemetry";
  statusTopic = base + "/status";
  availabilityTopic = base + "/availability";
  Serial.printf("\nBuilding noise microphone | %s\nMQTT telemetry: %s\n", deviceID, telemetryTopic.c_str());
  Serial.printf("Orange >= %.1f dB; red >= %.1f dB; reset hysteresis %.1f dB\n",
                settings.warningDb, settings.limitDb, Config::HYSTERESIS_DB);
  Serial.println("Estimated SPL only: unweighted 16 kHz samples, 500 ms RMS; no raw audio uploaded");
  micReady = startMicrophone();
  emitSnapshot();
  // Keep potentially blocking TCP/HTTP work off the audio task's core.
  if (xTaskCreatePinnedToCore(networkTask, "network", 10240, nullptr, 1, nullptr, 0) != pdPASS)
    Serial.println("Network task failed; microphone/LEDs continue locally");
}

void loop() {
  applyCommands();
  showState(state); // Animate between audio windows without blocking sensing.
  if (!micReady) {
    currentDbfs = NAN;
    currentClipped = false;
    emitSnapshot();
    delay(500);
    return;
  }
  size_t bytesRead = 0;
  const esp_err_t error = i2s_read(MIC_PORT, samples, sizeof(samples), &bytesRead, pdMS_TO_TICKS(100));
  static uint32_t lastAudioAt = millis();
  if (error != ESP_OK || bytesRead == 0) {
    if (uint32_t(millis() - lastAudioAt) >= 1000) {
      currentDbfs = NAN;
      currentClipped = false;
      window = AudioLevel::Window{};
      emitSnapshot();
      lastAudioAt = millis();
    }
    delay(1);
    return;
  }
  lastAudioAt = millis();
  for (size_t i = 0; i < bytesRead / sizeof(samples[0]); ++i) {
    window.add(AudioLevel::decode(samples[i]));
    if (window.count == Config::WINDOW_SAMPLES) {
      currentDbfs = static_cast<float>(window.dbfs());
      currentClipped = window.clipped;
      emitSnapshot();
      window = AudioLevel::Window{};
    }
  }
  delay(1);
}
