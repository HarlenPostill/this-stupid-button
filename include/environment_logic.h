#pragma once
#include <math.h>
#include <stdint.h>

namespace Environment {
inline float round2(float value) { return roundf(value * 100.0f) / 100.0f; }
enum class Status { NORMAL, HOT, DARK, HOT_DARK, SENSOR_FAULT };
inline Status classify(float c, float light, float hotC, float darkPercent) {
  if (!isfinite(c) || !isfinite(light)) return Status::SENSOR_FAULT;
  const bool hot = c > hotC, dark = light < darkPercent;
  return hot ? (dark ? Status::HOT_DARK : Status::HOT)
             : (dark ? Status::DARK : Status::NORMAL);
}
inline const char *name(Status s) {
  switch (s) {
    case Status::NORMAL: return "NORMAL";
    case Status::HOT: return "HOT";
    case Status::DARK: return "DARK";
    case Status::HOT_DARK: return "HOT & DARK";
    default: return "SENSOR_FAULT";
  }
}
inline int code(Status s) {
  switch (s) {
    case Status::NORMAL: return 0;
    case Status::HOT: return 1;
    case Status::DARK: return 2;
    case Status::HOT_DARK: return 3;
    default: return -1;
  }
}
// 3V3 -> fixed 10k -> GPIO34 node -> NTC -> GND.
inline float temperatureC(float mv, float supply, float fixed, float nominal,
                          float beta, float offset) {
  if (mv < 100 || mv > 3000 || mv >= supply || supply <= 0 ||
      fixed <= 0 || nominal <= 0 || beta <= 0) return NAN;
  const float resistance = fixed * mv / (supply - mv);
  const float c = 1.0f / (1.0f / 298.15f + logf(resistance / nominal) / beta)
                  - 273.15f + offset;
  return isfinite(c) && c >= -10 && c <= 80 ? c : NAN;
}
// Relative ADC full-scale percentage, NOT lux. LDR is on the 3V3 side.
inline float lightPercent(uint32_t raw) { return 100.0f * raw / 4095.0f; }
}
