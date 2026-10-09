#include <assert.h>
#include <stdio.h>
#include "audio_level.h"
#include "noise_logic.h"

int main() {
  // Independently known signal: full-scale sine, then one tenth its amplitude.
  AudioLevel::Window full, tenth, dc, silent;
  constexpr double pi = 3.14159265358979323846;
  for (unsigned i = 0; i < 16000; ++i) {
    const double wave = sin(2 * pi * 1000 * i / 16000);
    full.add(static_cast<int32_t>(8388607 * wave));
    tenth.add(static_cast<int32_t>(8388607 * 0.1 * wave + 100000));
    dc.add(12345);
    silent.add(0);
  }
  assert(fabs(full.dbfs()) < 0.001);
  assert(fabs(tenth.dbfs() + 20) < 0.001); // DC offset removed; 10x amplitude = 20 dB.
  assert(!isfinite(dc.dbfs()) && !isfinite(silent.dbfs()));
  assert(!isfinite(AudioLevel::Window{}.dbfs()));
  assert(full.clipped && !tenth.clipped);
  assert(AudioLevel::decode(0x12345600) == 0x123456);
  assert(AudioLevel::decode(static_cast<int32_t>(0xffffff00u)) == -1);
  AudioLevel::Window sensitivity;
  for (unsigned i = 0; i < 16000; ++i)
    sensitivity.add(static_cast<int32_t>(8388607 * pow(10, -26.0 / 20) * sin(2 * pi * 1000 * i / 16000)));
  assert(fabs(sensitivity.dbfs() + 120 - 94) < 0.001);

  Noise::Settings s;
  using Noise::State;
  assert(s.warningDb == 80 && s.limitDb == 95 && Noise::valid(s));
  assert(Noise::classify(65, State::Fault, s) == State::Safe);
  assert(Noise::classify(80, State::Safe, s) == State::Warning);
  assert(Noise::classify(95, State::Safe, s) == State::High);
  assert(Noise::classify(94, State::High, s) == State::High);
  assert(Noise::classify(93, State::High, s) == State::High);
  assert(Noise::classify(92.9, State::High, s) == State::Warning);
  assert(Noise::classify(78, State::Warning, s) == State::Warning);
  assert(Noise::classify(77.9, State::Warning, s) == State::Safe);
  assert(Noise::classify(50, State::High, s) == State::Safe);
  assert(Noise::classify(NAN, State::Safe, s) == State::Fault);
  assert(Noise::classify(INFINITY, State::High, s) == State::Fault);
  s.warningDb = 94; assert(!Noise::valid(s));
  s.warningDb = NAN; assert(!Noise::valid(s));
  s.warningDb = 80; s.offsetDb = 31; assert(!Noise::valid(s));
  s.offsetDb = 0; s.limitDb = 121; assert(!Noise::valid(s));
  puts("PASS: I2S decode, RMS/dB reference, DC removal, clipping, thresholds, hysteresis and faults");
}
