#include <Arduino.h>
#include <BLE2902.h>
#include <BLE2904.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>

// BLE-controlled sonar alarm for an ESP32, HC-SR04 and passive buzzer.
// Phone apps such as nRF Connect should display the device as "ESP32 Sonar Alarm".

namespace
{
// Hardware pins. IMPORTANT: reduce a 5 V HC-SR04 ECHO signal to 3.3 V with
// a resistor divider before it reaches GPIO 18.
constexpr uint8_t TRIG_PIN = 5;
constexpr uint8_t ECHO_PIN = 18;
constexpr uint8_t BUZZER_PIN = 25;

// HC-SR04 timing and filtering.
constexpr float SOUND_CM_PER_US = 0.0343f;
constexpr uint32_t ECHO_TIMEOUT_US = 25000;
constexpr uint8_t SONAR_SAMPLE_COUNT = 3;
constexpr uint32_t RANGE_UPDATE_INTERVAL_MS = 250; // four measurements/notifies per second
constexpr uint16_t INVALID_RANGE_CM = 0xFFFF;
constexpr uint16_t MIN_THRESHOLD_CM = 2;
constexpr uint16_t MAX_THRESHOLD_CM = 400;

// ESP32 LEDC hardware PWM. A passive buzzer is loudest at a 50% duty cycle;
// volume 0..100 is therefore mapped to duty 0..128 (not 0..255).
constexpr uint8_t PWM_CHANNEL = 0;
constexpr uint8_t PWM_RESOLUTION_BITS = 8;
constexpr uint16_t PWM_MAX_AUDIBLE_DUTY = 128;
constexpr uint16_t BUZZER_FREQUENCIES_HZ[3] = {800, 1600, 2400};

// One application service groups the measurement and all controls belonging
// to the same physical alarm. These are custom 128-bit UUIDs.
constexpr char DEVICE_NAME[] = "ESP32 Sonar Alarm";
constexpr char SERVICE_UUID[] = "8f1d0000-6f7b-4a9e-9c2d-34a1b2c3d4e5";
constexpr char RANGE_UUID[] = "8f1d0001-6f7b-4a9e-9c2d-34a1b2c3d4e5";
constexpr char ENABLE_UUID[] = "8f1d0002-6f7b-4a9e-9c2d-34a1b2c3d4e5";
constexpr char THRESHOLD_UUID[] = "8f1d0003-6f7b-4a9e-9c2d-34a1b2c3d4e5";
constexpr char TONE_UUID[] = "8f1d0004-6f7b-4a9e-9c2d-34a1b2c3d4e5";
constexpr char VOLUME_UUID[] = "8f1d0005-6f7b-4a9e-9c2d-34a1b2c3d4e5";

// Bluetooth SIG unit 0x2701 is metre. Exponent -2 means each integer count is
// 10^-2 m = 1 cm. Thus raw uint16 value 50 means 50 cm (0.50 m).
constexpr uint16_t BLE_UNIT_METRE = 0x2701;

volatile bool deviceConnected = false;
volatile bool configChanged = false;

// Shared application state. These small aligned values are atomic on ESP32.
volatile bool alarmEnabled = false;
volatile uint16_t thresholdCm = 50;
volatile uint8_t toneSelection = 1; // valid values 1, 2, 3
volatile uint8_t volumePercent = 50;
uint16_t rangeCm = INVALID_RANGE_CM;

BLEServer *bleServer = nullptr;
BLECharacteristic *rangeCharacteristic = nullptr;
BLECharacteristic *enableCharacteristic = nullptr;
BLECharacteristic *thresholdCharacteristic = nullptr;
BLECharacteristic *toneCharacteristic = nullptr;
BLECharacteristic *volumeCharacteristic = nullptr;

void setUint8(BLECharacteristic *characteristic, uint8_t value)
{
  characteristic->setValue(&value, sizeof(value));
}

void setUint16LE(BLECharacteristic *characteristic, uint16_t value)
{
  uint8_t bytes[2] = {
      static_cast<uint8_t>(value & 0xFF),
      static_cast<uint8_t>((value >> 8) & 0xFF)};
  characteristic->setValue(bytes, sizeof(bytes));
}

uint16_t readUint16LE(const std::string &value)
{
  return static_cast<uint16_t>(static_cast<uint8_t>(value[0])) |
         (static_cast<uint16_t>(static_cast<uint8_t>(value[1])) << 8);
}

void addUserDescription(BLECharacteristic *characteristic, const char *text)
{
  BLEDescriptor *description = new BLEDescriptor(BLEUUID(static_cast<uint16_t>(0x2901)));
  description->setAccessPermissions(ESP_GATT_PERM_READ);
  description->setValue(std::string(text));
  characteristic->addDescriptor(description);
}

void addCentimetrePresentation(BLECharacteristic *characteristic)
{
  BLE2904 *format = new BLE2904();
  format->setFormat(BLE2904::FORMAT_UINT16);
  format->setExponent(-2);
  format->setUnit(BLE_UNIT_METRE);
  format->setNamespace(1); // Bluetooth SIG assigned numbers
  format->setDescription(0);
  format->setAccessPermissions(ESP_GATT_PERM_READ);
  characteristic->addDescriptor(format);
}

class ServerCallbacks final : public BLEServerCallbacks
{
  void onConnect(BLEServer *) override
  {
    deviceConnected = true;
    Serial.println("BLE client connected");
  }

  void onDisconnect(BLEServer *) override
  {
    deviceConnected = false;
    Serial.println("BLE client disconnected");
  }
};

enum class Control : uint8_t
{
  Enable,
  Threshold,
  Tone,
  Volume
};

class ControlCallbacks final : public BLECharacteristicCallbacks
{
public:
  explicit ControlCallbacks(Control control) : control_(control) {}

  void onWrite(BLECharacteristic *characteristic) override
  {
    const std::string value = characteristic->getValue();
    bool accepted = false;

    switch (control_)
    {
    case Control::Enable:
      if (value.size() == 1 && static_cast<uint8_t>(value[0]) <= 1)
      {
        alarmEnabled = static_cast<uint8_t>(value[0]) == 1;
        Serial.printf("BLE write: alarm %s\n", alarmEnabled ? "enabled" : "disabled");
        accepted = true;
      }
      if (!accepted)
        setUint8(characteristic, alarmEnabled ? 1 : 0);
      break;

    case Control::Threshold:
      if (value.size() == 2)
      {
        const uint16_t requested = readUint16LE(value);
        if (requested >= MIN_THRESHOLD_CM && requested <= MAX_THRESHOLD_CM)
        {
          thresholdCm = requested;
          Serial.printf("BLE write: threshold %u cm\n", thresholdCm);
          accepted = true;
        }
      }
      if (!accepted)
        setUint16LE(characteristic, thresholdCm);
      break;

    case Control::Tone:
      if (value.size() == 1)
      {
        const uint8_t requested = static_cast<uint8_t>(value[0]);
        if (requested >= 1 && requested <= 3)
        {
          toneSelection = requested;
          Serial.printf("BLE write: tone %u (%u Hz)\n", toneSelection,
                        BUZZER_FREQUENCIES_HZ[toneSelection - 1]);
          accepted = true;
        }
      }
      if (!accepted)
        setUint8(characteristic, toneSelection);
      break;

    case Control::Volume:
      if (value.size() == 1 && static_cast<uint8_t>(value[0]) <= 100)
      {
        volumePercent = static_cast<uint8_t>(value[0]);
        Serial.printf("BLE write: volume %u%%\n", volumePercent);
        accepted = true;
      }
      if (!accepted)
        setUint8(characteristic, volumePercent);
      break;
    }

    if (!accepted)
      Serial.println("Rejected invalid BLE payload; characteristic restored");
    else
      configChanged = true;
  }

private:
  Control control_;
};

uint16_t measureOneRangeCm()
{
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  const uint32_t echoUs = pulseIn(ECHO_PIN, HIGH, ECHO_TIMEOUT_US);
  if (echoUs == 0)
    return INVALID_RANGE_CM;

  const float distance = echoUs * SOUND_CM_PER_US / 2.0f;
  if (distance < MIN_THRESHOLD_CM || distance > MAX_THRESHOLD_CM)
    return INVALID_RANGE_CM;
  return static_cast<uint16_t>(distance + 0.5f);
}

uint16_t measureRangeCm()
{
  uint16_t samples[SONAR_SAMPLE_COUNT];
  uint8_t validCount = 0;

  for (uint8_t i = 0; i < SONAR_SAMPLE_COUNT; ++i)
  {
    const uint16_t sample = measureOneRangeCm();
    if (sample != INVALID_RANGE_CM)
      samples[validCount++] = sample;
    delay(5);
  }

  if (validCount == 0)
    return INVALID_RANGE_CM;

  // Insertion sort is compact and gives a median that rejects stray echoes.
  for (uint8_t i = 1; i < validCount; ++i)
  {
    const uint16_t item = samples[i];
    int8_t j = i - 1;
    while (j >= 0 && samples[j] > item)
    {
      samples[j + 1] = samples[j];
      --j;
    }
    samples[j + 1] = item;
  }
  return samples[validCount / 2];
}

void updateBuzzer()
{
  const bool shouldSound = alarmEnabled && rangeCm != INVALID_RANGE_CM &&
                           rangeCm < thresholdCm && volumePercent > 0;
  if (!shouldSound)
  {
    ledcWrite(PWM_CHANNEL, 0);
    return;
  }

  const uint16_t frequency = BUZZER_FREQUENCIES_HZ[toneSelection - 1];
  const uint32_t duty = (static_cast<uint32_t>(volumePercent) *
                         PWM_MAX_AUDIBLE_DUTY) /
                        100;
  ledcChangeFrequency(PWM_CHANNEL, frequency, PWM_RESOLUTION_BITS);
  ledcWrite(PWM_CHANNEL, duty);
}

void createGattServer()
{
  BLEDevice::init(DEVICE_NAME);
  bleServer = BLEDevice::createServer();
  bleServer->setCallbacks(new ServerCallbacks());

  // Five characteristics plus their descriptors need more than the default
  // service handle allocation, so reserve 32 handles.
  BLEService *service = bleServer->createService(BLEUUID(std::string(SERVICE_UUID)), 32);

  rangeCharacteristic = service->createCharacteristic(
      RANGE_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);
  rangeCharacteristic->setAccessPermissions(ESP_GATT_PERM_READ);
  rangeCharacteristic->addDescriptor(new BLE2902()); // CCCD enables phone subscription
  addUserDescription(rangeCharacteristic,
                     "Range: uint16 little-endian cm; 0xFFFF means no valid echo");
  addCentimetrePresentation(rangeCharacteristic);
  setUint16LE(rangeCharacteristic, INVALID_RANGE_CM);

  enableCharacteristic = service->createCharacteristic(
      ENABLE_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE);
  enableCharacteristic->setAccessPermissions(ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE);
  enableCharacteristic->setCallbacks(new ControlCallbacks(Control::Enable));
  addUserDescription(enableCharacteristic, "Alarm enable: uint8, 0=off, 1=on");
  setUint8(enableCharacteristic, alarmEnabled ? 1 : 0);

  thresholdCharacteristic = service->createCharacteristic(
      THRESHOLD_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE);
  thresholdCharacteristic->setAccessPermissions(ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE);
  thresholdCharacteristic->setCallbacks(new ControlCallbacks(Control::Threshold));
  addUserDescription(thresholdCharacteristic,
                     "Detection threshold: uint16 little-endian cm, valid 2..400");
  addCentimetrePresentation(thresholdCharacteristic);
  setUint16LE(thresholdCharacteristic, thresholdCm);

  toneCharacteristic = service->createCharacteristic(
      TONE_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE);
  toneCharacteristic->setAccessPermissions(ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE);
  toneCharacteristic->setCallbacks(new ControlCallbacks(Control::Tone));
  addUserDescription(toneCharacteristic, "Tone: uint8 1=800Hz, 2=1600Hz, 3=2400Hz");
  setUint8(toneCharacteristic, toneSelection);

  volumeCharacteristic = service->createCharacteristic(
      VOLUME_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_WRITE);
  volumeCharacteristic->setAccessPermissions(ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE);
  volumeCharacteristic->setCallbacks(new ControlCallbacks(Control::Volume));
  addUserDescription(volumeCharacteristic, "Volume: uint8 percent, 0..100");
  setUint8(volumeCharacteristic, volumePercent);

  service->start();

  BLEAdvertising *advertising = BLEDevice::getAdvertising();
  advertising->addServiceUUID(SERVICE_UUID);
  advertising->setScanResponse(true);
  advertising->setMinPreferred(0x06);
  advertising->setMaxPreferred(0x12);
  BLEDevice::startAdvertising();
}
} // namespace

void setup()
{
  Serial.begin(115200);

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  digitalWrite(TRIG_PIN, LOW);

  ledcSetup(PWM_CHANNEL, BUZZER_FREQUENCIES_HZ[0], PWM_RESOLUTION_BITS);
  ledcAttachPin(BUZZER_PIN, PWM_CHANNEL);
  ledcWrite(PWM_CHANNEL, 0);

  createGattServer();
  Serial.println("ESP32 Sonar Alarm BLE server ready and advertising");
  Serial.println("Defaults: disabled, threshold 50 cm, tone 1 (800 Hz), volume 50%");
}

void loop()
{
  static uint32_t lastRangeUpdateMs = 0;
  static bool wasConnected = false;
  static uint32_t disconnectedAtMs = 0;
  const uint32_t now = millis();

  // Restart advertising after a disconnect. The short delay lets the BLE stack
  // finish cleaning up the previous connection.
  if (!deviceConnected && wasConnected)
  {
    disconnectedAtMs = now;
    wasConnected = false;
  }
  if (!deviceConnected && disconnectedAtMs != 0 && now - disconnectedAtMs >= 500)
  {
    bleServer->startAdvertising();
    disconnectedAtMs = 0;
    Serial.println("BLE advertising restarted");
  }
  if (deviceConnected)
    wasConnected = true;

  if (now - lastRangeUpdateMs >= RANGE_UPDATE_INTERVAL_MS)
  {
    lastRangeUpdateMs = now;
    rangeCm = measureRangeCm();
    setUint16LE(rangeCharacteristic, rangeCm);
    updateBuzzer();

    if (deviceConnected)
      rangeCharacteristic->notify();

    if (rangeCm == INVALID_RANGE_CM)
      Serial.println("Range: no valid echo; alarm off");
    else
      Serial.printf("Range: %u cm, threshold: %u cm, alarm output: %s\n",
                    rangeCm, thresholdCm,
                    (alarmEnabled && rangeCm < thresholdCm && volumePercent > 0) ? "ON" : "OFF");
  }

  // Apply writes promptly instead of waiting up to 250 ms for the next sample.
  if (configChanged)
  {
    configChanged = false;
    updateBuzzer();
  }

  delay(2);
}
