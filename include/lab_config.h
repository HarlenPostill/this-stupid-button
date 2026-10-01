#pragma once

// Copy these settings into include/secrets.h to keep Wi-Fi details out of git.
// Both programs work with placeholders for building, but need real Wi-Fi to connect.
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
#define LAB_GROUP_ID "harlen-group01" // Change to your own unique group label; no / + # spaces.
#endif

namespace Config {
constexpr char WIFI_SSID[] = LAB_WIFI_SSID;
constexpr char WIFI_PASSWORD[] = LAB_WIFI_PASSWORD;
constexpr char GROUP_ID[] = LAB_GROUP_ID;
constexpr char MQTT_HOST[] = "broker.emqx.io";
constexpr unsigned short MQTT_PORT = 1883;
// Same public demonstration credentials as lecture slide 32.
constexpr char MQTT_USER[] = "emqx";
constexpr char MQTT_PASSWORD[] = "public";
constexpr char ACK_COMMAND[] = "ack"; // Tutor exercise: change to "silence" and re-upload.
constexpr float TRIGGER_C = 30.0f;
constexpr float HYSTERESIS_C = 2.0f; // Reset at <= 28 C with default trigger.
constexpr unsigned long ESCALATION_MS = 2000;
constexpr float SERIES_OHMS = 10000.0f;
constexpr float NTC_NOMINAL_OHMS = 10000.0f; // Verify kit thermistor specification.
constexpr float NTC_BETA = 3950.0f;
constexpr float SUPPLY_MV = 3300.0f; // Can replace with measured 3V3 rail voltage.
constexpr float TEMP_OFFSET_C = 0.0f;
constexpr unsigned char THERMISTOR_PIN = 34; // ADC1, works while Wi-Fi is active.
constexpr unsigned char BUZZER_PIN = 25;
constexpr unsigned char BUTTON_PIN = 27; // Switch to GND; internal pull-up.
}
