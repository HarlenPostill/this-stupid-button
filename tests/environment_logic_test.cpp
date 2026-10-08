#include "environment_logic.h"
#include <assert.h>
#include <stdio.h>

int main() {
  using namespace Environment;
  assert(classify(25, 70, 30, 25) == Status::NORMAL);
  assert(classify(31, 70, 30, 25) == Status::HOT);
  assert(classify(25, 10, 30, 25) == Status::DARK);
  assert(classify(31, 10, 30, 25) == Status::HOT_DARK);
  assert(classify(30, 25, 30, 25) == Status::NORMAL); // Both exact boundaries.
  assert(classify(30.01f, 25, 30, 25) == Status::HOT);
  assert(classify(30, 24.99f, 30, 25) == Status::DARK);
  assert(classify(round2(30.004f), round2(24.999f), 30, 25) == Status::NORMAL);
  assert(classify(round2(30.006f), round2(24.994f), 30, 25) == Status::HOT_DARK);
  assert(classify(29, 20, 30, 25) == Status::DARK);
  assert(classify(29, 20, 28, 25) == Status::HOT_DARK); // Tutor changes hot.
  assert(classify(29, 20, 28, 15) == Status::HOT);       // Tutor changes dark.
  assert(classify(NAN, 50, 30, 25) == Status::SENSOR_FAULT);
  assert(classify(25, NAN, 30, 25) == Status::SENSOR_FAULT);
  assert(classify(INFINITY, 50, 30, 25) == Status::SENSOR_FAULT);
  assert(code(Status::HOT_DARK) == 3 && code(Status::SENSOR_FAULT) == -1);
  assert(fabsf(temperatureC(1650, 3300, 10000, 10000, 3950, 0) - 25) < 0.01f);
  assert(temperatureC(1400, 3300, 10000, 10000, 3950, 0) > 25);
  assert(temperatureC(1900, 3300, 10000, 10000, 3950, 0) < 25);
  assert(isnan(temperatureC(0, 3300, 10000, 10000, 3950, 0)));
  assert(isnan(temperatureC(3300, 3300, 10000, 10000, 3950, 0)));
  assert(isnan(temperatureC(1650, 3300, 10000, 10000, 0, 0)));
  assert(lightPercent(0) == 0 && lightPercent(4095) == 100);
  assert(fabsf(lightPercent(2048) - 50.0122f) < 0.01f);
  puts("PASS: four statuses, strict boundaries, live threshold predictions, faults, NTC direction and light scaling");
}
