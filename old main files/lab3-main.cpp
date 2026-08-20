#include <Arduino.h>

// ======================================================
// Pin configuration
// ======================================================

const int LED_PIN = 18;
const int POWER_BUTTON_PIN = 32;
const int BRIGHTNESS_BUTTON_PIN = 26;

// ======================================================
// Lamp state
// ======================================================

bool lampOn = false;

enum Brightness {
  BRIGHTNESS_LOW,
  BRIGHTNESS_MEDIUM,
  BRIGHTNESS_HIGH
};

Brightness brightness = BRIGHTNESS_LOW;

// ======================================================
// PWM settings
// ======================================================

const unsigned long PWM_FREQUENCY = 1000; // 1000 Hz

// Period = 1 / frequency
// 1000 Hz = 1000 microseconds per cycle
const unsigned long PWM_PERIOD_US = 1000000UL / PWM_FREQUENCY;

// Initial brightness = LOW
float dutyCycle = 0.25;

// ======================================================
// Button debounce settings
// ======================================================

const unsigned long DEBOUNCE_TIME = 50; // milliseconds

bool lastPowerReading = HIGH;
bool stablePowerState = HIGH;
unsigned long lastPowerChange = 0;

bool lastBrightnessReading = HIGH;
bool stableBrightnessState = HIGH;
unsigned long lastBrightnessChange = 0;

// ======================================================
// Function declarations
// ======================================================

void handlePowerButton();
void handleBrightnessButton();
void softwarePWM();
void printLampStatus();

// ======================================================
// Setup
// ======================================================

void setup() {
  pinMode(LED_PIN, OUTPUT);

  // Using internal pull-up resistors:
  // Button released = HIGH
  // Button pressed  = LOW
  pinMode(POWER_BUTTON_PIN, INPUT_PULLUP);
  pinMode(BRIGHTNESS_BUTTON_PIN, INPUT_PULLUP);

  // Different baud rate from lecture demo
  Serial.begin(9600);

  // Required initial state:
  // Lamp OFF
  // Brightness LOW
  lampOn = false;
  brightness = BRIGHTNESS_LOW;
  dutyCycle = 0.25;

  digitalWrite(LED_PIN, LOW);

  printLampStatus();
}

// ======================================================
// Main loop
// ======================================================

void loop() {
  handlePowerButton();
  handleBrightnessButton();
  softwarePWM();
}

// ======================================================
// Power button
// ======================================================

void handlePowerButton() {
  bool reading = digitalRead(POWER_BUTTON_PIN);

  // If raw reading changes, restart debounce timer
  if (reading != lastPowerReading) {
    lastPowerChange = millis();
    lastPowerReading = reading;
  }

  // Only accept the new state after it has remained
  // unchanged for the debounce period
  if ((millis() - lastPowerChange) > DEBOUNCE_TIME) {

    if (reading != stablePowerState) {
      stablePowerState = reading;

      // Only react when the button is pressed
      // INPUT_PULLUP means pressed = LOW
      if (stablePowerState == LOW) {
        lampOn = !lampOn;

        printLampStatus();
      }
    }
  }
}

// ======================================================
// Brightness button
// ======================================================

void handleBrightnessButton() {
  bool reading = digitalRead(BRIGHTNESS_BUTTON_PIN);

  // Restart debounce timer if reading changes
  if (reading != lastBrightnessReading) {
    lastBrightnessChange = millis();
    lastBrightnessReading = reading;
  }

  // Wait until button state is stable
  if ((millis() - lastBrightnessChange) > DEBOUNCE_TIME) {

    if (reading != stableBrightnessState) {
      stableBrightnessState = reading;

      // Only react once when button is pressed
      if (stableBrightnessState == LOW) {

        switch (brightness) {

          case BRIGHTNESS_LOW:
            brightness = BRIGHTNESS_MEDIUM;
            dutyCycle = 0.50;
            break;

          case BRIGHTNESS_MEDIUM:
            brightness = BRIGHTNESS_HIGH;
            dutyCycle = 0.75;
            break;

          case BRIGHTNESS_HIGH:
            brightness = BRIGHTNESS_LOW;
            dutyCycle = 0.25;
            break;
        }

        printLampStatus();
      }
    }
  }
}

// ======================================================
// Software PWM
// ======================================================

void softwarePWM() {

  // If lamp is OFF, force LED LOW
  if (!lampOn) {
    digitalWrite(LED_PIN, LOW);
    return;
  }

  // Determine where we currently are within the PWM period
  unsigned long timeInPeriod = micros() % PWM_PERIOD_US;

  // Calculate how long LED should remain ON
  unsigned long onTime =
      (unsigned long)(PWM_PERIOD_US * dutyCycle);

  // Software-generated PWM
  if (timeInPeriod < onTime) {
    digitalWrite(LED_PIN, HIGH);
  } else {
    digitalWrite(LED_PIN, LOW);
  }
}

// ======================================================
// Serial output
// ======================================================

void printLampStatus() {

  Serial.println();
  Serial.println("--------------------");

  Serial.print("Lamp: ");

  if (lampOn) {
    Serial.println("ON");
  } else {
    Serial.println("OFF");
  }

  Serial.print("Brightness: ");

  switch (brightness) {

    case BRIGHTNESS_LOW:
      Serial.println("LOW");
      break;

    case BRIGHTNESS_MEDIUM:
      Serial.println("MEDIUM");
      break;

    case BRIGHTNESS_HIGH:
      Serial.println("HIGH");
      break;
  }

  Serial.print("PWM Frequency: ");
  Serial.print(PWM_FREQUENCY);
  Serial.println(" Hz");

  Serial.print("Duty Cycle: ");
  Serial.print(dutyCycle * 100.0);
  Serial.println(" %");

  // Approximate ESP32 GPIO HIGH voltage = 3.3 V
  float averageVoltage = 3.3 * dutyCycle;

  Serial.print("Average Voltage: ");
  Serial.print(averageVoltage, 2);
  Serial.println(" V");

  Serial.println("--------------------");
}