# INMP441 serial volume demo

Simple microphone test for the existing `upesy_wrover` ESP32 board. The active
firmware is `src/main.cpp`. It reads I2S audio at 16 kHz and prints volume every
500 ms at **115200 baud**. No extra libraries or Wi-Fi are needed.

## Wiring

Disconnect USB while wiring. Use GPIO labels, not header position numbers.

| INMP441 pin | ESP32 connection |
| --- | --- |
| VDD / VCC | **3V3** |
| GND | GND |
| SCK / BCLK | GPIO26 |
| WS / LRCLK | GPIO25 |
| SD / DOUT | GPIO33 |
| L/R | **GND** (left channel) |

Use 3.3 V, keep wires short, and leave the microphone's small sound hole clear.
L/R must be connected. Pin constants are at the top of `src/main.cpp`.
The firmware enables a pull-down on SD; the datasheet recommends a 100 kΩ
SD-to-GND resistor, which can also be fitted if the breakout lacks one. A bare
microphone needs supply decoupling (100 nF); breakout boards usually include it.

## Build, upload and monitor

```sh
pio run -e upesy_wrover
pio run -e upesy_wrover -t upload
pio device monitor -b 115200
```

If `pio` is not on PATH, use `~/.platformio/penv/bin/pio` or the PlatformIO
toolbar. Close the monitor before uploading. Speak or clap near the microphone;
readings should increase. A brief clap is averaged over the 500 ms window.

Example output format (illustrative, not a measured hardware result):

```text
Volume: 55.0 dB SPL (estimate) | Level: -65.0 dBFS
```

## What the dB values mean

- **dBFS** measures digital audio level. Louder audio moves toward 0 dBFS.
  The code removes DC offset and calculates RMS over 8000 samples, using the
  datasheet convention where a full-scale sine wave is 0 dBFS.
- **dB SPL (estimate)** uses nominal sensitivity of -26 dBFS at 94 dB SPL:
  `estimated SPL = dBFS + 120 + SPL_OFFSET_DB`. This is an uncalibrated,
  unweighted estimate over the sampled bandwidth, not dBA. Microphone sensitivity
  tolerance and the breakout affect accuracy. Compare with a reference meter
  using a steady tone, then set `SPL_OFFSET_DB` in `src/main.cpp` to reference
  reading minus displayed reading to improve the estimate.
- **CLIPPING** means samples reached at least 99% of digital full scale; reduce
  the sound level or move the microphone farther away.
- **No varying audio** means samples are all zero or constant. Check power,
  SD and L/R wiring. Quiet surroundings still produce microphone noise.

Reference: [TDK INMP441 datasheet, pages 11–12](https://product.tdk.cn/system/files/dam/doc/product/sw_piezo/mic/mems-mic/data_sheet/inmp441.pdf).
The pinned platform uses Arduino ESP32 2.0.17 and its
[legacy I2S driver](https://docs.espressif.com/projects/esp-idf/en/v4.4.1/esp32/api-reference/peripherals/i2s.html).

## Archived lab

The previous environmental monitoring project, including its original firmware,
configuration, guide, queries and tests, is preserved in
[`old main files/lab-10`](old%20main%20files/lab-10/README.md). Its local secrets
copy remains ignored by Git. Build the old lab independently with:

```sh
pio run -d "old main files/lab-10" -e upesy_wrover
```

Optional host check for the audio calculation:

```sh
c++ -std=c++11 -Wall -Wextra -Werror -Iinclude tests/audio_level_test.cpp -o /tmp/inmp441_audio_test
/tmp/inmp441_audio_test
```
