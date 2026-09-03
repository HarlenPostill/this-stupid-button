#include <Arduino.h>

/*
 * W6: Touch-controlled servo
 *
 * Connections (ESP32):
 *   ON/OFF touch pad    -> GPIO4  (T0)
 *   move towards 180 deg-> GPIO13 (T4)
 *   move towards 0 deg  -> GPIO14 (T6)
 *   servo signal        -> GPIO25
 *   servo red wire      -> suitable external 5 V supply
 *   servo brown/black   -> supply GND AND ESP32 GND
 *
 * Do not power a servo from the ESP32 3.3 V pin. GPIO numbers and touch
 * thresholds can be changed independently in the constants below.
 */

constexpr uint8_t TOUCH_TOGGLE_PIN = 4;
constexpr uint8_t TOUCH_FORWARD_PIN = 13;
constexpr uint8_t TOUCH_REVERSE_PIN = 14;
constexpr uint8_t SERVO_PIN = 25;

// A classic ESP32 touch reading normally FALLS when the pad is touched.
// Replace these three starting values with thresholds determined from your
// Teleplot traces. A useful threshold lies clearly between the touched and
// untouched groups of readings; each input can have a different value.
constexpr uint16_t TOGGLE_THRESHOLD = 30;
constexpr uint16_t FORWARD_THRESHOLD = 30;
constexpr uint16_t REVERSE_THRESHOLD = 30;

// Replace 42 with the last two digits of one group member's student ID.
constexpr long STUDENT_ID_LAST_TWO_DIGITS = 42;
const int MOVEMENT_STEPS =
    map(STUDENT_ID_LAST_TWO_DIGITS, 0, 99, 5, 10);
static_assert(STUDENT_ID_LAST_TWO_DIGITS >= 0 &&
                  STUDENT_ID_LAST_TWO_DIGITS <= 99,
              "The last two ID digits must be in the range 0 to 99");

constexpr uint8_t PWM_CHANNEL = 0;
constexpr uint32_t PWM_FREQUENCY_HZ = 50;
constexpr uint8_t PWM_RESOLUTION_BITS = 11; // lab requires 11 or 12 bits
constexpr uint32_t PWM_LEVELS = 1UL << PWM_RESOLUTION_BITS;

// At 50 Hz, one period is 20 ms. Starting with the common theoretical servo
// range 0.5 ms to 2.5 ms:
//   duty = pulseWidth / 20 ms * 2^resolution
// For 11 bits: 0.5/20*2048 ~= 51 and 2.5/20*2048 ~= 256.
constexpr uint32_t THEORETICAL_DUTY_0_DEG =
    (PWM_LEVELS * 500UL + 10000UL) / 20000UL;
constexpr uint32_t THEORETICAL_DUTY_180_DEG =
    (PWM_LEVELS * 2500UL + 10000UL) / 20000UL;

// Experimentally adjust these two values until this particular servo reaches
// its real 0 and 180 degree positions without buzzing or pushing past a stop.
constexpr uint32_t CALIBRATED_DUTY_0_DEG = THEORETICAL_DUTY_0_DEG;
constexpr uint32_t CALIBRATED_DUTY_180_DEG = THEORETICAL_DUTY_180_DEG;
static_assert(CALIBRATED_DUTY_0_DEG < CALIBRATED_DUTY_180_DEG,
              "Servo endpoint duties must increase from 0 to 180 degrees");
static_assert(CALIBRATED_DUTY_180_DEG < PWM_LEVELS,
              "Servo duty must fit the selected PWM resolution");

constexpr uint32_t TOUCH_SAMPLE_INTERVAL_MS = 10;
constexpr uint32_t TOUCH_DEBOUNCE_MS = 50;
constexpr uint32_t TELEPLOT_INTERVAL_MS = 50;

// Debounces both touch and release. update() returns true only once per physical
// touch, after the new state has remained stable for TOUCH_DEBOUNCE_MS.
struct TouchSwitch {
  uint8_t pin;
  uint16_t threshold;
  bool stablePressed = false;
  bool candidatePressed = false;
  uint32_t candidateSince = 0;
  uint16_t reading = 0;

  TouchSwitch(uint8_t touchPin, uint16_t touchThreshold)
      : pin(touchPin), threshold(touchThreshold)
  {
  }

  bool update(uint32_t now)
  {
    reading = touchRead(pin);
    const bool rawPressed = reading < threshold;

    if (rawPressed != candidatePressed) {
      candidatePressed = rawPressed;
      candidateSince = now;
    }

    if (candidatePressed != stablePressed &&
        now - candidateSince >= TOUCH_DEBOUNCE_MS) {
      stablePressed = candidatePressed;
      return stablePressed; // event on the pressed edge only
    }
    return false;
  }
};

TouchSwitch toggleSwitch{TOUCH_TOGGLE_PIN, TOGGLE_THRESHOLD};
TouchSwitch forwardSwitch{TOUCH_FORWARD_PIN, FORWARD_THRESHOLD};
TouchSwitch reverseSwitch{TOUCH_REVERSE_PIN, REVERSE_THRESHOLD};

bool servoControlOn = false;
int positionIndex = 0; // 0..MOVEMENT_STEPS inclusive

uint32_t dutyForIndex(int index)
{
  // Linear mapping makes index 0 the calibrated 0 degree duty and index N
  // the calibrated 180 degree duty. Using index steps avoids accumulated error.
  return static_cast<uint32_t>(map(index, 0, MOVEMENT_STEPS,
                                   CALIBRATED_DUTY_0_DEG,
                                   CALIBRATED_DUTY_180_DEG));
}

void writeServoPosition()
{
  ledcWrite(PWM_CHANNEL, dutyForIndex(positionIndex));
}

void setup()
{
  Serial.begin(115200);

  // Hardware LEDC PWM: channel, frequency in Hz, and resolution in bits.
  ledcSetup(PWM_CHANNEL, PWM_FREQUENCY_HZ, PWM_RESOLUTION_BITS);
  // Route the selected LEDC hardware channel to the servo signal GPIO.
  ledcAttachPin(SERVO_PIN, PWM_CHANNEL);
  writeServoPosition();

  Serial.printf("ID digits=%ld, movement steps N=%d\n",
                STUDENT_ID_LAST_TWO_DIGITS, MOVEMENT_STEPS);
  Serial.printf("PWM=%lu Hz, %u-bit; theoretical duties=%lu..%lu\n",
                static_cast<unsigned long>(PWM_FREQUENCY_HZ),
                PWM_RESOLUTION_BITS,
                static_cast<unsigned long>(THEORETICAL_DUTY_0_DEG),
                static_cast<unsigned long>(THEORETICAL_DUTY_180_DEG));
}

void loop()
{
  static uint32_t lastTouchSample = 0;
  static uint32_t lastTeleplotOutput = 0;
  const uint32_t now = millis();

  if (now - lastTouchSample >= TOUCH_SAMPLE_INTERVAL_MS) {
    lastTouchSample = now;

    const bool toggleEvent = toggleSwitch.update(now);
    const bool forwardEvent = forwardSwitch.update(now);
    const bool reverseEvent = reverseSwitch.update(now);

    if (toggleEvent) {
      servoControlOn = !servoControlOn;
    }

    // Movement touches are still read/debounced while OFF, but are ignored.
    if (servoControlOn) {
      if (forwardEvent && !reverseEvent && positionIndex < MOVEMENT_STEPS) {
        ++positionIndex;
        writeServoPosition();
      } else if (reverseEvent && !forwardEvent && positionIndex > 0) {
        --positionIndex;
        writeServoPosition();
      }
    }
  }

  if (now - lastTeleplotOutput >= TELEPLOT_INTERVAL_MS) {
    lastTeleplotOutput = now;

    // Teleplot format is >series:value. These first three traces provide the
    // evidence needed to select a separate threshold for every touch input.
    Serial.printf(">Touch_Toggle:%u\n", toggleSwitch.reading);
    Serial.printf(">Touch_Forward:%u\n", forwardSwitch.reading);
    Serial.printf(">Touch_Reverse:%u\n", reverseSwitch.reading);
    Serial.printf(">Threshold_Toggle:%u\n", TOGGLE_THRESHOLD);
    Serial.printf(">Threshold_Forward:%u\n", FORWARD_THRESHOLD);
    Serial.printf(">Threshold_Reverse:%u\n", REVERSE_THRESHOLD);
    Serial.printf(">Control_On:%u\n", servoControlOn ? 1U : 0U);
    Serial.printf(">Position_Index:%d\n", positionIndex);
    Serial.printf(">Servo_Duty:%lu\n",
                  static_cast<unsigned long>(dutyForIndex(positionIndex)));
  }
}
