#pragma once
#include <stdint.h>
#include <math.h>

enum class AlarmState { NORMAL, WARNING, EMERGENCY, SENSOR_FAULT };
inline const char *stateName(AlarmState s) {
  switch (s) {
    case AlarmState::NORMAL: return "NORMAL";
    case AlarmState::WARNING: return "WARNING";
    case AlarmState::EMERGENCY: return "EMERGENCY";
    default: return "SENSOR_FAULT";
  }
}

// All alarm state belongs to the Arduino loop task, never to the MQTT task.
struct AlarmLogic {
  AlarmState state = AlarmState::NORMAL;
  bool acknowledged = false;
  uint32_t enteredAt = 0;
  uint32_t episode = 0;
  uint32_t revision = 0;

  void enter(AlarmState next, uint32_t now) {
    if (state == next) return;
    if (next == AlarmState::WARNING || next == AlarmState::SENSOR_FAULT) ++episode;
    state = next;
    enteredAt = now;
    acknowledged = false;
    ++revision;
  }
  void sample(float c, uint32_t now, float trigger, float hysteresis) {
    if (!isfinite(c)) { enter(AlarmState::SENSOR_FAULT, now); return; }
    if (state == AlarmState::SENSOR_FAULT) enter(AlarmState::NORMAL, now);
    if (state == AlarmState::NORMAL) {
      if (c > trigger) enter(AlarmState::WARNING, now);
    } else if (c <= trigger - hysteresis) {
      enter(AlarmState::NORMAL, now);
    }
  }
  void tick(uint32_t now, uint32_t escalation) {
    if (state == AlarmState::WARNING && !acknowledged &&
        uint32_t(now - enteredAt) >= escalation) enter(AlarmState::EMERGENCY, now);
  }
  bool acknowledge() {
    if (state == AlarmState::NORMAL || acknowledged) return false;
    acknowledged = true;
    ++revision;
    return true;
  }
  unsigned buzzerHz(uint32_t now) const {
    if (state == AlarmState::NORMAL || acknowledged) return 0;
    const uint32_t phase = now - enteredAt;
    if (state == AlarmState::WARNING) return phase % 1000 < 200 ? 1800 : 0;
    return phase % 250 < 125 ? 2800 : 0;
  }
};

// Wiring: 3V3 -> fixed 10k -> ADC node -> NTC -> GND.
inline float thermistorC(float mv, float supply, float fixed, float nominal,
                         float beta, float offset) {
  if (mv < 100 || mv > 3000 || mv >= supply) return NAN;
  const float resistance = fixed * mv / (supply - mv);
  const float c = 1.0f / (1.0f / 298.15f + logf(resistance / nominal) / beta)
                  - 273.15f + offset;
  return isfinite(c) && c >= -10 && c <= 80 ? c : NAN;
}
