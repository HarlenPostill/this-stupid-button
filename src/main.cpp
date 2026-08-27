#include <Arduino.h>

// Potentiometer connections:
//   one outer pin -> 3.3 V, other outer pin -> GND, wiper -> GPIO34
// LED connections:
//   GPIO25 -> current-limiting resistor -> LED anode, LED cathode -> GND
constexpr uint8_t POT_PIN = 34;       // ADC1 pin; safe to use as an analogue input
constexpr uint8_t LED_PIN = 25;       // PWM-capable output pin
constexpr uint8_t PWM_CHANNEL = 0;

constexpr uint8_t ADC_BITS = 12;
constexpr int ADC_MAX = (1 << ADC_BITS) - 1;  // 4095
constexpr float ADC_REFERENCE_V = 3.3F;

constexpr uint32_t PWM_FREQUENCY_HZ = 5000;
constexpr uint8_t PWM_BITS = 8;
constexpr int PWM_MAX = (1 << PWM_BITS) - 1;  // 255
constexpr int PWM_MIN_ON = 20;                // visible minimum brightness

// Readings at or below this value keep the lamp completely off.
// 400/4095 is about 9.8% of the potentiometer travel (about 0.32 V).
constexpr int ADC_OFF_THRESHOLD = 400;

constexpr uint32_t SAMPLE_INTERVAL_MS = 100;

void setup()
{
  Serial.begin(115200);

  analogReadResolution(ADC_BITS);
  pinMode(POT_PIN, INPUT);

  ledcSetup(PWM_CHANNEL, PWM_FREQUENCY_HZ, PWM_BITS);
  ledcAttachPin(LED_PIN, PWM_CHANNEL);
  ledcWrite(PWM_CHANNEL, 0);
}

void loop()
{
  static uint32_t lastSampleTime = 0;
  const uint32_t now = millis();

  if (now - lastSampleTime < SAMPLE_INTERVAL_MS) {
    return;
  }
  lastSampleTime = now;

  const int adcReading = analogRead(POT_PIN);
  const float measuredVoltage =
      adcReading * (ADC_REFERENCE_V / static_cast<float>(ADC_MAX));

  const bool lampOn = adcReading > ADC_OFF_THRESHOLD;
  int pwmDuty = 0;

  if (lampOn) {
    // Convert ADC values 401..4095 into 8-bit PWM values 20..255.
    pwmDuty = map(adcReading,
                  ADC_OFF_THRESHOLD + 1, ADC_MAX,
                  PWM_MIN_ON, PWM_MAX);
    pwmDuty = constrain(pwmDuty, PWM_MIN_ON, PWM_MAX);
  }

  ledcWrite(PWM_CHANNEL, pwmDuty);

  // Teleplot format: >variable:value
  Serial.printf(">ADC:%d\n", adcReading);
  Serial.printf(">Voltage:%.3f\n", measuredVoltage);
  Serial.printf(">PWM_Duty:%d\n", pwmDuty);
  Serial.printf(">Lamp_Status:%d\n", lampOn ? 1 : 0);
}
