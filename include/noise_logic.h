#pragma once
#include <math.h>
#include <stdint.h>

namespace Noise {
enum class State : uint8_t { Safe = 0, Warning = 1, High = 2, Fault = 3 };
inline const char *name(State state) {
  switch (state) {
    case State::Safe: return "safe";
    case State::Warning: return "warning";
    case State::High: return "high";
    default: return "sensor_fault";
  }
}
struct Settings {
  float warningDb = 80;
  float limitDb = 95;
  float offsetDb = 0;
  bool calibrated = false;
};
inline bool valid(const Settings &s, float hysteresis = 2) {
  return isfinite(s.warningDb) && isfinite(s.limitDb) && isfinite(s.offsetDb) &&
      s.warningDb >= 30 && s.limitDb <= 120 &&
      s.limitDb - s.warningDb > hysteresis &&
      s.offsetDb >= -30 && s.offsetDb <= 30;
}
inline State classify(float spl, State previous, const Settings &s, float hysteresis = 2) {
  if (!isfinite(spl)) return State::Fault;
  if (spl >= s.limitDb) return State::High; // Escalate immediately.
  if (previous == State::High && spl >= s.limitDb - hysteresis) return State::High;
  if (spl >= s.warningDb) return State::Warning;
  if ((previous == State::Warning || previous == State::High) &&
      spl >= s.warningDb - hysteresis) return State::Warning;
  return State::Safe;
}
}
