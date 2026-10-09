#pragma once
#include <math.h>
#include <stdint.h>

namespace AudioLevel {
// Signed 24-bit audio is left aligned in the 32-bit I2S word.
inline int32_t decode(int32_t word) { return word >> 8; }

struct Window {
  double sum = 0;
  double sumSquares = 0;
  uint32_t count = 0;
  bool clipped = false;

  void add(int32_t sample) {
    const double value = sample;
    sum += value;
    sumSquares += value * value;
    ++count;
    if (sample >= 8304722 || sample <= -8304722) clipped = true;
  }
  double dbfs() const {
    if (!count) return -INFINITY;
    const double mean = sum / count;
    const double power = sumSquares / count - mean * mean;
    if (power <= 0) return -INFINITY;
    // Datasheet dBFS reference: full-scale sine RMS = 2^23 / sqrt(2).
    constexpr double fullScaleSinePower = 8388608.0 * 8388608.0 / 2.0;
    return 10.0 * log10(power / fullScaleSinePower);
  }
};
}
