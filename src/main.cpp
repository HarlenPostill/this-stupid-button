#include <Arduino.h>
#include <driver/i2s.h>
#include <esp_err.h>
#include "audio_level.h"

namespace {
constexpr i2s_port_t MIC_PORT = I2S_NUM_0;
constexpr int MIC_SCK = 26;
constexpr int MIC_WS = 25;
constexpr int MIC_SD = 33;
constexpr uint32_t SAMPLE_RATE = 16000;
constexpr uint32_t WINDOW_SAMPLES = SAMPLE_RATE / 2; // Print every 500 ms.
constexpr double SPL_OFFSET_DB = 0.0; // Adjust against a reference meter.
int32_t samples[256];
AudioLevel::Window window;
bool micReady = false;

bool startMicrophone() {
  i2s_config_t config{};
  config.mode = static_cast<i2s_mode_t>(I2S_MODE_MASTER | I2S_MODE_RX);
  config.sample_rate = SAMPLE_RATE;
  config.bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT;
  config.channel_format = I2S_CHANNEL_FMT_ONLY_LEFT; // L/R tied to GND.
  config.communication_format = I2S_COMM_FORMAT_STAND_I2S;
  config.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
  config.dma_buf_count = 8;
  config.dma_buf_len = 256;
  config.use_apll = false;
  config.bits_per_chan = I2S_BITS_PER_CHAN_32BIT; // 64 clocks per stereo frame.

  esp_err_t error = i2s_driver_install(MIC_PORT, &config, 0, nullptr);
  if (error != ESP_OK) {
    Serial.printf("I2S install failed: %s\n", esp_err_to_name(error));
    return false;
  }
  i2s_pin_config_t pins{};
  pins.mck_io_num = I2S_PIN_NO_CHANGE; // INMP441 does not need MCLK.
  pins.bck_io_num = MIC_SCK;
  pins.ws_io_num = MIC_WS;
  pins.data_out_num = I2S_PIN_NO_CHANGE;
  pins.data_in_num = MIC_SD;
  error = i2s_set_pin(MIC_PORT, &pins);
  if (error != ESP_OK) {
    Serial.printf("I2S pin setup failed: %s\n", esp_err_to_name(error));
    i2s_driver_uninstall(MIC_PORT);
    return false;
  }
  // SD floats outside the microphone's 24 data bits; keep unused bits low.
  pinMode(MIC_SD, INPUT_PULLDOWN);
  // Drain startup samples while the microphone settles (~300 ms).
  const uint32_t started = millis();
  while (millis() - started < 300) {
    size_t bytesRead = 0;
    i2s_read(MIC_PORT, samples, sizeof(samples), &bytesRead, pdMS_TO_TICKS(100));
  }
  return true;
}

void printLevel() {
  const double dbfs = window.dbfs();
  if (!isfinite(dbfs)) {
    Serial.println("No varying audio: check 3V3, GND, SD and L/R=GND wiring.");
  } else {
    // Nominal sensitivity: -26 dBFS at 94 dB SPL, hence SPL = dBFS + 120.
    const double estimatedSpl = dbfs + 120.0 + SPL_OFFSET_DB;
    Serial.printf("Volume: %.1f dB SPL (estimate) | Level: %.1f dBFS%s\n",
                  estimatedSpl, dbfs, window.clipped ? " | CLIPPING" : "");
  }
  window = AudioLevel::Window{};
}
} // namespace

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\nINMP441 volume demo");
  Serial.println("VDD=3V3 GND=GND SCK=GPIO26 WS=GPIO25 SD=GPIO33 L/R=GND");
  Serial.println("16 kHz audio, 500 ms RMS windows; SPL is uncalibrated and unweighted.");
  micReady = startMicrophone();
  if (!micReady) Serial.println("Microphone setup failed. Reset after checking the error above.");
}

void loop() {
  if (!micReady) {
    delay(1000);
    return;
  }
  size_t bytesRead = 0;
  const esp_err_t error = i2s_read(MIC_PORT, samples, sizeof(samples), &bytesRead,
                                   pdMS_TO_TICKS(1000));
  if (error != ESP_OK || bytesRead == 0) {
    Serial.printf("I2S read failed (%s, %u bytes). Check wiring.\n",
                  esp_err_to_name(error), static_cast<unsigned>(bytesRead));
    window = AudioLevel::Window{};
    delay(100);
    return;
  }
  for (size_t i = 0; i < bytesRead / sizeof(samples[0]); ++i) {
    window.add(AudioLevel::decode(samples[i]));
    if (window.count == WINDOW_SAMPLES) printLevel();
  }
}
