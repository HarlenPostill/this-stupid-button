#include "alarm_logic.h"
#include <assert.h>
#include <stdio.h>

int main() {
  AlarmLogic a;
  a.sample(30, 0, 30, 2);
  assert(a.state == AlarmState::NORMAL); // Strictly exceeds.
  a.sample(30.1f, 100, 30, 2);
  assert(a.state == AlarmState::WARNING && a.episode == 1);
  assert(a.buzzerHz(100) == 1800 && a.buzzerHz(300) == 0);
  a.tick(2099, 2000);
  assert(a.state == AlarmState::WARNING);
  a.tick(2100, 2000);
  assert(a.state == AlarmState::EMERGENCY && a.episode == 1);
  assert(a.buzzerHz(2100) == 2800 && a.buzzerHz(2225) == 0);
  assert(a.acknowledge() && !a.acknowledge());
  assert(a.buzzerHz(2350) == 0 && a.state == AlarmState::EMERGENCY);
  a.sample(28.1f, 2400, 30, 2);
  assert(a.state == AlarmState::EMERGENCY);
  a.sample(28, 2500, 30, 2);
  assert(a.state == AlarmState::NORMAL && !a.acknowledged);
  assert(!a.acknowledge());
  a.sample(31, 3000, 30, 2);
  assert(a.episode == 2 && a.acknowledge());
  a.tick(9000, 2000);
  assert(a.state == AlarmState::WARNING && a.buzzerHz(9000) == 0);
  a.sample(28, 9100, 30, 2);
  a.sample(31, UINT32_MAX - 999, 30, 2);
  a.tick(999, 2000);
  assert(a.state == AlarmState::WARNING);
  a.tick(1000, 2000);
  assert(a.state == AlarmState::EMERGENCY); // millis rollover.
  a.sample(NAN, 2000, 30, 2);
  assert(a.state == AlarmState::SENSOR_FAULT && a.buzzerHz(2000) == 2800);
  a.acknowledge();
  a.sample(31, 2100, 30, 2);
  assert(a.state == AlarmState::WARNING && !a.acknowledged);
  a.sample(20, 2200, 30, 2);
  assert(a.state == AlarmState::NORMAL);
  assert(fabsf(thermistorC(1650,3300,10000,10000,3950,0) - 25) < 0.01);
  assert(thermistorC(1400,3300,10000,10000,3950,0) > 25);
  assert(isnan(thermistorC(0,3300,10000,10000,3950,0)));
  assert(isnan(thermistorC(3100,3300,10000,10000,3950,0)));
  puts("PASS: thresholds, hysteresis, escalation, ack, rearm, buzzer, rollover, sensor fault and conversion");
}
