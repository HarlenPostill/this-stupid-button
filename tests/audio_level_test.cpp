#include "audio_level.h"
#include <assert.h>
#include <stdio.h>

AudioLevel::Window sine(double amplitude, double dc = 0) {
  AudioLevel::Window window;
  for (int i = 0; i < 8000; ++i) {
    const double phase = 2.0 * 3.141592653589793 * 1000.0 * i / 16000.0;
    window.add(static_cast<int32_t>(lround(amplitude * sin(phase) + dc)));
  }
  return window;
}

int main() {
  assert(AudioLevel::decode(0x12345600) == 0x123456);
  assert(AudioLevel::decode(INT32_MIN) == -8388608);
  assert(AudioLevel::decode(-256) == -1);
  assert(fabs(sine(8388607).dbfs()) < 0.001);
  assert(fabs(sine(4194304).dbfs() + 6.0206) < 0.001);
  // Manufacturer's nominal 94 dB SPL calibration tone is -26 dBFS.
  const double amplitude = 8388608.0 * pow(10.0, -26.0 / 20.0);
  assert(fabs(sine(amplitude).dbfs() + 120.0 - 94.0) < 0.001);
  assert(fabs(sine(100000, 250000).dbfs() - sine(100000).dbfs()) < 0.001);
  AudioLevel::Window quiet;
  assert(isinf(quiet.dbfs()) && quiet.dbfs() < 0);
  for (int i = 0; i < 8000; ++i) quiet.add(12345);
  assert(isinf(quiet.dbfs()) && quiet.dbfs() < 0);
  assert(sine(8388607).clipped);
  assert(!sine(100000).clipped);
  puts("PASS: signed I2S decoding, sine reference, dB scaling, SPL estimate, DC removal, silence and clipping");
}
