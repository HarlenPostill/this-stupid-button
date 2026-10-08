#pragma once
#include <stdint.h>

#if __has_include("secrets.h")
#include "secrets.h"
#endif
#ifndef LAB_WIFI_SSID
#define LAB_WIFI_SSID "YOUR_2.4_GHZ_WIFI"
#endif
#ifndef LAB_WIFI_PASSWORD
#define LAB_WIFI_PASSWORD "YOUR_WIFI_PASSWORD"
#endif
#ifndef LAB_GROUP_ID
#define LAB_GROUP_ID "YOUR_GROUP_ID"
#endif
#ifndef LAB_STUDENT_ID
#define LAB_STUDENT_ID "YOUR_STUDENT_ID"
#endif
#ifndef LAB_INFLUXDB_URL
#define LAB_INFLUXDB_URL "https://YOUR_REGION.aws.cloud2.influxdata.com"
#endif
#ifndef LAB_INFLUXDB_ORG
#define LAB_INFLUXDB_ORG "YOUR_ORG_ID"
#endif
#ifndef LAB_INFLUXDB_BUCKET
#define LAB_INFLUXDB_BUCKET "environment_lab"
#endif
#ifndef LAB_INFLUXDB_TOKEN
#define LAB_INFLUXDB_TOKEN "YOUR_BUCKET_WRITE_TOKEN"
#endif

namespace Config {
constexpr char WIFI_SSID[] = LAB_WIFI_SSID;
constexpr char WIFI_PASSWORD[] = LAB_WIFI_PASSWORD;
constexpr char GROUP_ID[] = LAB_GROUP_ID;
constexpr char STUDENT_ID[] = LAB_STUDENT_ID;
constexpr char INFLUX_URL[] = LAB_INFLUXDB_URL;
constexpr char INFLUX_ORG[] = LAB_INFLUXDB_ORG;
constexpr char INFLUX_BUCKET[] = LAB_INFLUXDB_BUCKET;
constexpr char INFLUX_TOKEN[] = LAB_INFLUXDB_TOKEN;
constexpr char MEASUREMENT[] = "environment";
constexpr char ROOM[] = "lab_bench";
constexpr char ZONE[] = "ground";
constexpr char DEVICE_MODEL[] = "ESP32_WROVER";
constexpr char SENSOR_TYPES[] = "NTC10k_LDR";
constexpr char FIRMWARE_VERSION[] = "1.0.0";
constexpr char TZ_INFO[] = "AEST-10AEDT,M10.1.0,M4.1.0/3";
constexpr uint8_t THERMISTOR_PIN = 34; // ADC1: works while Wi-Fi is active.
constexpr uint8_t LIGHT_PIN = 35;      // ADC1, input-only, no internal pulls.
constexpr uint32_t SAMPLE_MS = 1000;
constexpr uint32_t UPLOAD_MS = 15000;
constexpr float HOT_C = 30.0f;
constexpr float DARK_PERCENT = 25.0f;
constexpr float SUPPLY_MV = 3300.0f;
constexpr float FIXED_OHMS = 10000.0f;
constexpr float NTC_NOMINAL_OHMS = 10000.0f; // Your part is labelled 10 kΩ.
constexpr float NTC_BETA = 3950.0f;          // Assumed kit part, not auto-detected.
constexpr float TEMP_OFFSET_C = 0.0f;
constexpr bool HOMEWORK_ONLY = false; // true: temperature-only lecture practice.
}
