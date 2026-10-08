#include <Arduino.h>
#include <WiFi.h>
#include <InfluxDbClient.h>
#include <InfluxDbCloud.h>
#include <esp_timer.h>
#include <esp_sntp.h>
#include <atomic>
#include <sys/time.h>
#include "lab_config.h"
#include "environment_logic.h"

// Week 9 slides 38-41: InfluxDBClient + cloud CA, Point, tags, clearFields,
// addField and writePoint. A separate network task keeps sensing at 1 Hz
// even when HTTPS takes several seconds. Only that task owns the client.
namespace {
struct Reading {
  float temperature, light, hotThreshold, darkThreshold;
  uint32_t thermistorMv, lightRaw;
  uint64_t epochMs, uptimeS;
  Environment::Status status;
};
QueueHandle_t readings;
float hotThreshold = Environment::round2(Config::HOT_C);
float darkThreshold = Environment::round2(Config::DARK_PERCENT);
uint32_t lastSample;
char serialCommand[80];
size_t serialLength = 0;
bool serialOverflow = false;
std::atomic<bool> timeSynchronized{false};

bool clockReady() {
  return timeSynchronized.load() && time(nullptr) >= 1735689600; // >= 2025-01-01 UTC.
}
bool isPlaceholder(const char *s) { return !s[0] || strstr(s, "YOUR_"); }
bool configurationReady() {
  return !isPlaceholder(Config::WIFI_SSID) && !isPlaceholder(Config::GROUP_ID) &&
         !isPlaceholder(Config::STUDENT_ID) && !isPlaceholder(Config::INFLUX_URL) &&
         !isPlaceholder(Config::INFLUX_ORG) && !isPlaceholder(Config::INFLUX_BUCKET) &&
         !isPlaceholder(Config::INFLUX_TOKEN) &&
         strncmp(Config::INFLUX_URL, "https://", 8) == 0;
}

void printHelp() {
  Serial.printf("Commands (newline): hot 29.5 | dark 25 | help\n"
                "HOT if T > %.2f C; DARK if light < %.2f %%\n",
                hotThreshold, darkThreshold);
}
void handleCommand(const char *text) {
  char word[16], extra;
  float value;
  if (strcmp(text, "help") == 0) { printHelp(); return; }
  // Third conversion rejects trailing junk; NaN and infinity are rejected.
  if (sscanf(text, "%15s %f %c", word, &value, &extra) != 2 || !isfinite(value)) {
    Serial.println("Invalid command. Use hot <C>, dark <percent>, or help.");
    return;
  }
  if (strcmp(word, "hot") == 0 && value >= -10 && value <= 80) hotThreshold = Environment::round2(value);
  else if (strcmp(word, "dark") == 0 && value >= 0 && value <= 100) darkThreshold = Environment::round2(value);
  else { Serial.println("Rejected: hot must be -10..80 C; dark 0..100 percent."); return; }
  printHelp(); // Next 1-second sample uses the new threshold; reboot resets it.
}
void pollSerial() {
  for (unsigned i = 0; i < 80 && Serial.available(); ++i) {
    const char c = Serial.read();
    if (c == '\r') continue;
    if (c == '\n') {
      serialCommand[serialLength] = '\0';
      if (serialOverflow) Serial.println("Command too long; discarded.");
      else if (serialLength) handleCommand(serialCommand);
      serialLength = 0;
      serialOverflow = false;
    } else if (serialLength < sizeof(serialCommand) - 1) serialCommand[serialLength++] = c;
    else serialOverflow = true;
  }
}

void sample() {
  uint32_t tempSum = 0, lightSum = 0;
  for (unsigned i = 0; i < 16; ++i) {
    tempSum += analogReadMilliVolts(Config::THERMISTOR_PIN);
    if (!Config::HOMEWORK_ONLY) lightSum += analogRead(Config::LIGHT_PIN);
  }
  Reading r{};
  r.thermistorMv = tempSum / 16;
  r.lightRaw = lightSum / 16;
  r.temperature = Environment::temperatureC(r.thermistorMv, Config::SUPPLY_MV,
      Config::FIXED_OHMS, Config::NTC_NOMINAL_OHMS, Config::NTC_BETA, Config::TEMP_OFFSET_C);
  // Classify the same two-decimal values stored in InfluxDB, so rounded fields
  // cannot appear equal to a threshold while the hidden reading exceeds it.
  r.temperature = Environment::round2(r.temperature);
  r.light = Environment::round2(Environment::lightPercent(r.lightRaw));
  r.hotThreshold = hotThreshold;
  r.darkThreshold = darkThreshold;
  r.status = Environment::classify(r.temperature, r.light, r.hotThreshold, r.darkThreshold);
  r.uptimeS = static_cast<uint64_t>(esp_timer_get_time()) / 1000000ULL;
  if (clockReady()) {
    timeval tv;
    gettimeofday(&tv, nullptr);
    r.epochMs = static_cast<uint64_t>(tv.tv_sec) * 1000ULL + tv.tv_usec / 1000;
  }
  xQueueOverwrite(readings, &r); // Latest reading only; no offline history queue.
  if (Config::HOMEWORK_ONLY)
    Serial.printf("HOMEWORK T=%.2f C ADC=%lu mV UTC_ms=%llu\n", r.temperature,
                  static_cast<unsigned long>(r.thermistorMv), r.epochMs);
  else
    Serial.printf("T=%.2f C light=%.2f %% raw=%lu state=%s UTC_ms=%llu\n",
                  r.temperature, r.light, static_cast<unsigned long>(r.lightRaw),
                  Environment::name(r.status), r.epochMs);
}

void networkTask(void *) {
  if (!configurationReady()) {
    Serial.println("Cloud disabled: complete include/secrets.h (see example). Local sensing continues.");
    vTaskDelete(nullptr);
    return;
  }
  InfluxDBClient client(Config::INFLUX_URL, Config::INFLUX_ORG,
      Config::INFLUX_BUCKET, Config::INFLUX_TOKEN, InfluxDbCloud2CACert);
  // Disable the library's additional retry buffer; our loop paces fresh writes.
  client.setWriteOptions(WriteOptions().writePrecision(WritePrecision::MS)
      .batchSize(1).bufferSize(1).retryInterval(0).maxRetryAttempts(0));
  client.setHTTPOptions(HTTPOptions().httpReadTimeout(5000).connectionReuse(true));
  Point sensor(Config::HOMEWORK_ONLY ? "lecture_temperature" : Config::MEASUREMENT);
  WiFi.mode(WIFI_STA);
  String deviceID = WiFi.macAddress(); // ESP32 station MAC, NOT WiFi.BSSIDstr().
  deviceID.replace(":", "");
  sensor.addTag("group_id", Config::GROUP_ID);
  sensor.addTag("student_id", Config::STUDENT_ID);
  sensor.addTag("device_id", deviceID);
  sensor.addTag("room", Config::ROOM);
  sensor.addTag("zone", Config::ZONE);
  sensor.addTag("device_model", Config::DEVICE_MODEL);
  sensor.addTag("sensor_types", Config::HOMEWORK_ONLY ? "NTC10k" : Config::SENSOR_TYPES);
  Serial.printf("Device %s | group %s | student %s | bucket %s\n",
                deviceID.c_str(), Config::GROUP_ID, Config::STUDENT_ID, Config::INFLUX_BUCKET);
  WiFi.begin(Config::WIFI_SSID, Config::WIFI_PASSWORD);
  uint32_t wifiRetry = millis(), lastAttempt = millis() - Config::UPLOAD_MS;
  bool ntpStarted = false, validated = false;
  Reading latest{};
  bool haveReading = false;
  for (;;) {
    if (xQueueReceive(readings, &latest, 0) == pdTRUE) haveReading = true;
    const uint32_t now = millis();
    if (WiFi.status() != WL_CONNECTED) {
      validated = false;
      if (uint32_t(now - wifiRetry) >= 10000) {
        wifiRetry = now;
        WiFi.reconnect();
        Serial.println("Wi-Fi retry: check 2.4 GHz network and credentials.");
      }
    } else {
      if (!ntpStarted) {
        // Same Sydney TZ and NTP principle as slide 40; async, no sensing pause.
        sntp_set_time_sync_notification_cb([](struct timeval *) {
          timeSynchronized.store(true); // Require an actual NTP reply this boot.
        });
        configTzTime(Config::TZ_INFO, "pool.ntp.org", "time.nis.gov");
        ntpStarted = true;
        Serial.println("Wi-Fi connected; waiting for NTP before any cloud writes.");
      }
      if (uint32_t(now - lastAttempt) >= Config::UPLOAD_MS) {
        lastAttempt = now;
        if (!clockReady() || !haveReading || latest.epochMs == 0) {
          Serial.println("Upload skipped: waiting for a sensor reading with valid UTC time.");
        } else if (Config::HOMEWORK_ONLY && !isfinite(latest.temperature)) {
          Serial.println("Upload skipped: check thermistor wiring.");
        } else {
          if (!validated) {
            validated = client.validateConnection(); // Same validation API as lecture.
            if (!validated) Serial.println("InfluxDB validation failed: " + client.getLastErrorMessage());
          }
          if (validated) {
            // Validation can take time: consume the newest completed sample.
            xQueueReceive(readings, &latest, 0);
            if (!latest.epochMs || (Config::HOMEWORK_ONLY && !isfinite(latest.temperature))) {
              Serial.println("Upload skipped: newest reading is not ready.");
              lastAttempt = millis();
              vTaskDelay(pdMS_TO_TICKS(20));
              continue;
            }
            sensor.clearFields(); // Tags stay; fields never accumulate across writes.
            if (isfinite(latest.temperature)) sensor.addField("temperature_c", latest.temperature, 2);
            if (!Config::HOMEWORK_ONLY) {
              sensor.addField("light_percent", latest.light, 2);
              sensor.addField("environment_status", Environment::name(latest.status));
              sensor.addField("status_code", Environment::code(latest.status));
              sensor.addField("sensor_ok", isfinite(latest.temperature));
              sensor.addField("light_adc_raw", static_cast<unsigned long>(latest.lightRaw));
              sensor.addField("temperature_threshold_c", latest.hotThreshold, 2);
              sensor.addField("light_threshold_percent", latest.darkThreshold, 2);
            }
            sensor.addField("thermistor_mv", static_cast<unsigned long>(latest.thermistorMv));
            sensor.addField("wifi_rssi_dbm", WiFi.RSSI());
            sensor.addField("uptime_s", static_cast<unsigned long long>(latest.uptimeS));
            sensor.addField("sample_interval_s", static_cast<unsigned long>(Config::SAMPLE_MS / 1000));
            sensor.addField("upload_interval_s", static_cast<unsigned long>(Config::UPLOAD_MS / 1000));
            sensor.addField("firmware_version", Config::FIRMWARE_VERSION);
            sensor.setTime(static_cast<unsigned long long>(latest.epochMs));
            Serial.println("Writing: " + sensor.toLineProtocol());
            if (client.writePoint(sensor)) Serial.println("InfluxDB write OK");
            else {
              Serial.println("InfluxDB write failed: " + client.getLastErrorMessage());
              validated = false;
            }
          }
        }
        // Pace from completion too: a slow request never creates a retry burst.
        lastAttempt = millis();
      }
    }
    vTaskDelay(pdMS_TO_TICKS(20));
  }
}
}

void setup() {
  Serial.begin(115200);
  analogReadResolution(12);
  pinMode(Config::THERMISTOR_PIN, INPUT);
  analogSetPinAttenuation(Config::THERMISTOR_PIN, ADC_11db);
  if (!Config::HOMEWORK_ONLY) {
    pinMode(Config::LIGHT_PIN, INPUT);
    analogSetPinAttenuation(Config::LIGHT_PIN, ADC_11db);
  }
  readings = xQueueCreate(1, sizeof(Reading));
  if (!readings) { Serial.println("Reading queue allocation failed"); abort(); }
  Serial.println("\nESP32 InfluxDB environmental monitor | Week 9 lecture basis");
  printHelp();
  lastSample = millis() - Config::SAMPLE_MS;
  if (xTaskCreate(networkTask, "influx", 12288, nullptr, 1, nullptr) != pdPASS)
    Serial.println("Network task unavailable; local sensing continues.");
}

void loop() {
  pollSerial();
  const uint32_t now = millis();
  if (uint32_t(now - lastSample) >= Config::SAMPLE_MS) {
    lastSample = now;
    sample();
  }
  delay(1);
}
