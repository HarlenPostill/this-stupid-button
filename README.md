# ESP32 InfluxDB environmental monitoring lab

The active firmware is [src/main.cpp](src/main.cpp). Start with
[LAB10_GUIDE.md](LAB10_GUIDE.md) for the complete wiring, homework, cloud setup,
schema explanation and assessment walkthrough. The supplied PDF is Week 9's
lecture; the upcoming monitoring lab is labelled lab-10 here to distinguish it
from your archived lab-9 MQTT work.

- Hardware: existing `upesy_wrover` ESP32 configuration, 10 kΩ NTC thermistor,
  photoresistor and **two fixed 10 kΩ resistors**.
- Sensing: GPIO34 temperature, GPIO35 light; one averaged sample each second.
- Upload: one related multi-field point every 15 seconds or slower over HTTPS,
  with explicit NTP-based UTC millisecond sample timestamps.
- Status: NORMAL, HOT, DARK, HOT & DARK; SENSOR_FAULT for invalid temperature.
- Threshold controls: Serial Monitor at 115200 baud, newline commands `hot 29`
  and `dark 25`. Changes are recorded alongside each uploaded reading.
- Previous lab: [old main files/lab-9](old%20main%20files/lab-9/README.md),
  with its independent PlatformIO configuration and original guide/PDF.

Your existing ignored `include/secrets.h` is retained. Add missing definitions
from [include/secrets.h.example](include/secrets.h.example); do not replace
existing Wi-Fi settings accidentally. The firmware keeps sensing locally if
cloud settings are incomplete. It prints no Wi-Fi password or API token.

```sh
pio run -e upesy_wrover
pio run -e upesy_wrover -t upload
pio device monitor -b 115200 --echo
c++ -std=c++11 -Wall -Wextra -Werror -Iinclude tests/environment_logic_test.cpp -o /tmp/environment_logic_test
/tmp/environment_logic_test
```

If `pio` is not on your terminal PATH, use
`~/.platformio/penv/bin/pio` or the PlatformIO VS Code toolbar. No cloud account
has been created and no physical readings or assessment screenshots are
included. Capture those using the guide on your assembled hardware.
