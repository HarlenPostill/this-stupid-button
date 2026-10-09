#pragma once
#include "noise_config.h"

namespace CabinetConfig {
constexpr uint8_t SERVO_PIN = 18;
constexpr uint8_t TRIG_PIN = 26;
constexpr uint8_t ECHO_PIN = 27; // HC-SR04 ECHO needs the external voltage divider.
constexpr uint8_t PWM_CHANNEL = 0;
// Conservative SG90 pulse range. Actual travel depends on the particular servo;
// verify unloaded and adjust the endpoints on the page before mounting the horn.
constexpr uint16_t SERVO_MIN_US = 1000;
constexpr uint16_t SERVO_MAX_US = 2000;
constexpr uint32_t ECHO_TIMEOUT_US = 25000;
constexpr uint32_t SAMPLE_MS = 100; // HC-SR04 needs >60 ms between triggers.
constexpr uint32_t NOISE_STALE_MS = 2500;
constexpr uint32_t DOOR_SETTLE_MS = 600;
constexpr float DOOR_HYSTERESIS_CM = 2;
constexpr char SOURCE_DEVICE[] = "noise-mic-F42DC976B9E4";
constexpr char LOCATION[] = "breaker_box";
constexpr char FIRMWARE[] = "1.0.0";
}
