#include <Arduino.h>
#include <WiFi.h>

// Set this to the exact SSID of your phone hotspot so the scan can find it.
constexpr char TARGET_SSID[] = "harlen";

constexpr uint8_t TRIG_PIN = 5;
constexpr uint8_t ECHO_PIN = 18;
constexpr uint8_t LED_PIN = 25;

// ----------------------------- Ultrasonic sensor ----------------------------

// Speed of sound in air ~343 m/s at 20 C = 0.0343 cm/us. The echo pulse covers
// the round trip (there and back), so the one-way distance divides by two:
//   distance_cm = echo_us * SOUND_CM_PER_US / 2
constexpr float SOUND_CM_PER_US = 0.0343f;
constexpr uint32_t ECHO_TIMEOUT_US = 25000; // ~4.3 m ceiling; avoids blocking
constexpr uint8_t DISTANCE_SAMPLE_COUNT = 5; // median rejects occasional bad echoes

// ----------------------------- LED PWM (LEDC) -------------------------------

constexpr uint8_t PWM_CHANNEL = 0;
constexpr uint32_t PWM_FREQUENCY_HZ = 5000; // well above flicker perception
constexpr uint8_t PWM_RESOLUTION_BITS = 8;  // 0..255 duty range
constexpr uint32_t PWM_DUTY_MAX = (1UL << PWM_RESOLUTION_BITS) - 1;

// ----------------------------- SIR -> brightness ----------------------------
//
// Choose these two from your own survey (Teleplot MIN/MAX over the movement
// range). Values below SIR_MIN map to the dimmest LED, values above SIR_MAX map
// to the brightest LED, and the span in between maps linearly onto the duty.
constexpr float SIR_MIN_DB = 5.0f;
constexpr float SIR_MAX_DB = 40.0f;

// ----------------------------- Scan cadence ---------------------------------

constexpr uint32_t DISTANCE_INTERVAL_MS = 100; // ranging is cheap, do it often
constexpr uint32_t SCAN_INTERVAL_MS = 1500;    // WiFi scans are comparatively slow

// Clear the serial terminal before printing each new measurement snapshot.
// This uses standard ANSI escape sequences supported by most serial terminals.
constexpr bool CLEAR_TERMINAL_EACH_FRAME = true;

// ----------------------------- Scan tuning ----------------------------------
//
// Scan parameters chosen for fast, repeatable co-channel measurements:
//   async      = false : block until results ready so the values used for SIR
//                        belong to one coherent snapshot.
//   show_hidden= false : we only need named APs for this experiment.
//   passive    = false : active probes make a phone hotspot easier to discover
//                        reliably during short scans.
//   max_ms_per_chan    : dwell time per channel. Long enough to catch a beacon
//                        (~100 ms beacon interval), short enough to stay snappy.
constexpr bool SCAN_ASYNC = false;
constexpr bool SCAN_HIDDEN = false;
// Active scans are more reliable for finding a phone hotspot during a short
// demonstration because the ESP32 sends probe requests instead of waiting only
// for periodic beacon frames.
constexpr bool SCAN_PASSIVE = false;
constexpr uint32_t SCAN_MAX_MS_PER_CHAN = 150;

// After the target channel is known we scan ONLY that channel. A full-band scan
// visits 13 channels; a single-channel scan is roughly an order of magnitude
// faster, which is the "apply channel info for efficiency" requirement.
int gTargetChannel = 0; // 0 = unknown yet -> scan all channels

// ----------------------------- Latest measurements --------------------------

float gDistanceCm = -1.0f;
int gPhoneRssi = 0;
int gInterfererRssi = 0;
bool gPhoneFound = false;
bool gInterfererFound = false;
float gSir = 0.0f;
String gVisibleSsids;
int gVisibleNetworkCount = 0;

// ---------------------------------------------------------------------------

float measureOneDistanceCm()
{
  // 10 us trigger pulse starts one ultrasonic burst.
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  // ECHO stays high for the round-trip flight time of the burst.
  const uint32_t echoUs = pulseIn(ECHO_PIN, HIGH, ECHO_TIMEOUT_US);
  if (echoUs == 0)
  {
    return -1.0f; // timeout: nothing in range
  }
  return echoUs * SOUND_CM_PER_US / 2.0f;
}

float measureDistanceCm()
{
  // Take several readings and return their median. A single HC-SR04 reading
  // can lock onto a nearby edge or a reflected path and jump unexpectedly.
  float samples[DISTANCE_SAMPLE_COUNT];
  uint8_t validCount = 0;

  for (uint8_t i = 0; i < DISTANCE_SAMPLE_COUNT; ++i)
  {
    const float sample = measureOneDistanceCm();
    if (sample >= 2.0f) // HC-SR04 specified minimum range is about 2 cm
    {
      samples[validCount++] = sample;
    }
    delay(5);
  }

  if (validCount == 0)
  {
    return -1.0f;
  }

  // Small insertion sort; the middle valid reading is the median.
  for (uint8_t i = 1; i < validCount; ++i)
  {
    const float value = samples[i];
    int8_t j = i - 1;
    while (j >= 0 && samples[j] > value)
    {
      samples[j + 1] = samples[j];
      --j;
    }
    samples[j + 1] = value;
  }
  return samples[validCount / 2];
}

// Runs one WiFi scan and updates the phone RSSI, target channel and strongest
// co-channel interferer. Returns true if the phone hotspot was seen.
bool scanLink()
{
  // Passing gTargetChannel (0 until known) makes the driver scan a single
  // channel once we have locked on, and the whole band before that.
  const int16_t count = WiFi.scanNetworks(SCAN_ASYNC, SCAN_HIDDEN, SCAN_PASSIVE,
                                          SCAN_MAX_MS_PER_CHAN, gTargetChannel);

  gPhoneFound = false;
  gInterfererFound = false;
  gPhoneRssi = 0;
  gInterfererRssi = -127; // start below any real RSSI so "strongest" works
  gVisibleSsids = "";
  gVisibleNetworkCount = count > 0 ? count : 0;

  // Retain a compact list for diagnostics after the terminal is cleared.
  for (int i = 0; i < count; ++i)
  {
    if (gVisibleSsids.length() > 0)
    {
      gVisibleSsids += ", ";
    }
    const String ssid = WiFi.SSID(i);
    gVisibleSsids += ssid.length() ? ssid : "<hidden>";
  }

  // First pass: locate the phone hotspot and (re)confirm its channel.
  int phoneChannel = gTargetChannel;
  int phoneIndex = -1;
  for (int i = 0; i < count; ++i)
  {
    if (WiFi.SSID(i) == TARGET_SSID)
    {
      phoneIndex = i;
      phoneChannel = WiFi.channel(i);
      gPhoneRssi = WiFi.RSSI(i);
      gPhoneFound = true;
      break;
    }
  }

  // Second pass: strongest co-channel AP that is NOT our hotspot. A larger
  // (less negative) RSSI is a stronger signal, so we keep the maximum.
  if (gPhoneFound)
  {
    gTargetChannel = phoneChannel; // lock scans to this channel from now on
    for (int i = 0; i < count; ++i)
    {
      if (i == phoneIndex)
      {
        continue; // exclude our own hotspot
      }
      if (WiFi.channel(i) == phoneChannel)
      {
        const int rssi = WiFi.RSSI(i);
        if (!gInterfererFound || rssi > gInterfererRssi)
        {
          gInterfererRssi = rssi;
          gInterfererFound = true;
        }
      }
    }
  }

  WiFi.scanDelete(); // free the result buffer for the next scan
  return gPhoneFound;
}

// Maps SIR onto the LED duty, clamping outside the chosen survey range.
uint32_t sirToDuty(float sir)
{
  if (sir <= SIR_MIN_DB)
  {
    return 0;
  }
  if (sir >= SIR_MAX_DB)
  {
    return PWM_DUTY_MAX;
  }
  const float frac = (sir - SIR_MIN_DB) / (SIR_MAX_DB - SIR_MIN_DB);
  return static_cast<uint32_t>(frac * PWM_DUTY_MAX + 0.5f);
}

void setup()
{
  Serial.begin(115200);

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  digitalWrite(TRIG_PIN, LOW);

  ledcSetup(PWM_CHANNEL, PWM_FREQUENCY_HZ, PWM_RESOLUTION_BITS);
  ledcAttachPin(LED_PIN, PWM_CHANNEL);
  // Full-brightness startup pulse proves the LED wiring independently of WiFi.
  ledcWrite(PWM_CHANNEL, PWM_DUTY_MAX);
  delay(750);
  ledcWrite(PWM_CHANNEL, 0);

  // Station mode, not connected: we only ever scan, never associate.
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  Serial.printf("Target SSID=\"%s\", SIR map=%.1f..%.1f dB -> duty 0..%lu\n",
                TARGET_SSID, SIR_MIN_DB, SIR_MAX_DB,
                static_cast<unsigned long>(PWM_DUTY_MAX));
}

void loop()
{
  static uint32_t lastDistance = 0;
  static uint32_t lastScan = 0;
  const uint32_t now = millis();

  if (now - lastDistance >= DISTANCE_INTERVAL_MS)
  {
    lastDistance = now;
    gDistanceCm = measureDistanceCm();
  }

  if (now - lastScan >= SCAN_INTERVAL_MS)
  {
    lastScan = now;
    scanLink();

    // SIR only meaningful when both are present; otherwise hold the LED dark.
    uint32_t duty = 0;
    if (gPhoneFound && gInterfererFound)
    {
      gSir = static_cast<float>(gPhoneRssi - gInterfererRssi);
      duty = sirToDuty(gSir);
    }
    else
    {
      gSir = 0.0f;
    }
    ledcWrite(PWM_CHANNEL, duty);

    // Teleplot: one snapshot per scan so all series line up in time.
    if (CLEAR_TERMINAL_EACH_FRAME)
    {
      Serial.print("\033[2J\033[H"); // clear screen, then move cursor to top-left
    }
    Serial.println("WiFi Link Quality Analyser");
    Serial.println("--------------------------");
    if (gDistanceCm >= 0.0f)
    {
      Serial.printf(">Distance_cm:%.1f\n", gDistanceCm);
    }
    Serial.printf(">Target_Channel:%d\n", gTargetChannel);
    if (!gPhoneFound)
    {
      Serial.printf("STATUS: Hotspot '%s' not found\n", TARGET_SSID);
      Serial.printf("Visible SSIDs (%d): %s\n", gVisibleNetworkCount,
                    gVisibleSsids.length() ? gVisibleSsids.c_str() : "none");
    }
    if (gPhoneFound)
    {
      Serial.printf(">Phone_RSSI:%d\n", gPhoneRssi);
    }
    if (gInterfererFound)
    {
      Serial.printf(">Interferer_RSSI:%d\n", gInterfererRssi);
    }
    if (gPhoneFound && gInterfererFound)
    {
      Serial.printf(">SIR_dB:%.1f\n", gSir);
      Serial.printf(">LED_Duty:%lu\n", static_cast<unsigned long>(duty));
    }
  }
}
