# BLE-Controlled Sonar Alarm - Assessment Guide

## Thirty-second project explanation

> The ESP32 is a BLE server that measures distance with an HC-SR04 ultrasonic
> sensor. A phone running nRF Connect is the BLE client. The phone can read and
> subscribe to the range, enable or disable the alarm, change its detection
> threshold, choose one of three buzzer frequencies, and set its volume. When
> the alarm is enabled and the valid measured range is less than the threshold,
> ESP32 LEDC hardware PWM drives the passive buzzer. Otherwise the buzzer is off.

## Key BLE terms

- **BLE server:** the ESP32. It owns the data, GATT service and characteristics,
  advertises as `ESP32 Sonar Alarm`, accepts a client connection, processes
  writes, and sends notifications.
- **BLE client:** the phone. It scans, connects, discovers the GATT database,
  reads and writes values, and subscribes to range notifications.
- **ATT:** the lower-level Attribute Protocol used to transfer attribute reads,
  writes and notifications between the client and server.
- **GATT:** organises ATT attributes into services, characteristics and
  descriptors, and defines how the application uses them.
- **Service:** a logical group of related characteristics. This project uses one
  service because every value belongs to the same sonar-alarm function.
- **Characteristic:** an application value with a UUID and properties such as
  Read, Write or Notify.
- **Descriptor:** metadata or configuration attached to a characteristic. This
  project uses descriptors for names, units/data format, and notification
  subscription.

## GATT design

Custom service UUID:
`8f1d0000-6f7b-4a9e-9c2d-34a1b2c3d4e5`

| UUID segment | Characteristic | Properties | Binary representation |
|---:|---|---|---|
| `0001` | Sonar range | Read, Notify | Little-endian `uint16` centimetres; `FFFF` means no valid echo |
| `0002` | Alarm enable | Read, Write | `uint8`: `00` off, `01` on |
| `0003` | Detection threshold | Read, Write | Little-endian `uint16` centimetres, valid range 2-400 |
| `0004` | Frequency selection | Read, Write | `uint8`: `01`=800 Hz, `02`=1600 Hz, `03`=2400 Hz |
| `0005` | Volume | Read, Write | `uint8`: 0-100 percent |

### Why these properties were selected

The range uses **Read** so a client can request the current measurement at any
time. It uses **Notify** so the server can push each new measurement to a
subscribed phone without the phone repeatedly polling. Notify is appropriate for
frequent live data because it has less overhead than an acknowledged indication.

The four controls use **Read** so a newly connected phone can inspect their
current settings. They use **Write** because the phone must change them. They do
not require Notify because the ESP32 is the only device that applies these
settings and the phone initiated each change.

### Descriptors

- Range has a Client Characteristic Configuration Descriptor (`0x2902`). The
  phone writes this automatically when the user enables notifications.
- Range and threshold have Characteristic Presentation Format descriptors
  (`0x2904`): format `uint16`, Bluetooth unit metre (`0x2701`), exponent -2.
  This defines each raw count as `10^-2 m`, which is exactly 1 cm.
- Characteristics have User Description descriptors (`0x2901`) that explain
  their payloads in readable text.

## Individual question 1: explain the GATT design and roles

Suggested answer:

> The ESP32 is the BLE server because it advertises and owns the GATT database.
> The phone is the client because it connects, reads and writes characteristics,
> and subscribes to updates. I used one custom service because the measurement
> and four controls all belong to one sonar alarm. Range is Read and Notify:
> Read provides the current value on demand, while Notify pushes live updates.
> Enable, threshold, frequency and volume are Read and Write because the phone
> needs to inspect and change them. Descriptors define readable names, the range
> format and unit, and the client's notification subscription.

If asked why there are not several services, explain that several services would
also work, but a single service is simpler and logically cohesive for this small
application. Separating unrelated functions would be useful in a larger device.

## Individual question 2: range representation and units

Suggested answer:

> The echo pulse duration is the ultrasonic sound's round-trip travel time. The
> code calculates one-way distance as `echo microseconds x 0.0343 / 2`. It rounds
> the filtered result to a 16-bit unsigned integer in centimetres. BLE sends the
> low byte first because the payload is little-endian. For example, 50 cm is
> decimal 50, hexadecimal `0x0032`, so the phone sends or receives `32 00`. The
> `0x2904` descriptor defines `uint16`, metre, exponent -2, which means centimetres.

The firmware takes three sonar samples and uses the median of the valid samples
to reject occasional reflected or stray echoes. `FFFF` represents no valid echo;
this can be reported to the phone but never activates the buzzer.

### If the tutor changes the unit to metres

Suggested answer:

> Range and threshold must always use the same representation and unit. If the
> range changes from centimetres to metres, a 50 cm measurement and threshold
> must both become 0.50 m. I would change both characteristic payload formats,
> both conversions, both descriptors, and the comparison variables together.
> Changing only the displayed range would cause incorrect alarm decisions.

One suitable redesign would use IEEE-754 `float32` metres for both range and
threshold and change the `0x2904` format to `FLOAT32` with metre exponent 0.

## Individual question 3: how phone writes control the buzzer

Suggested answer:

> A phone write is delivered by ATT to the selected GATT characteristic. The BLE
> library then calls my `ControlCallbacks::onWrite()` callback. The callback reads
> the byte array, checks its exact length and allowed range, and updates the
> corresponding ESP32 variable: `alarmEnabled`, `thresholdCm`, `toneSelection`,
> or `volumePercent`. It rejects invalid data and restores the characteristic's
> previous valid value. The main loop notices `configChanged` and immediately
> calls `updateBuzzer()`. LEDC then applies the selected hardware PWM frequency
> and duty cycle, or sets duty to zero when the alarm should be off.

The three pre-programmed frequencies are 800, 1600 and 2400 Hz. They are far
enough apart to be clearly distinguishable. Volume 0-100% maps to PWM duty
0-50%; a symmetrical 50% square wave provides the strongest drive for the
passive buzzer.

### Where to modify a requested control or payload

- Enable validation is in the `Control::Enable` case of `onWrite()`.
- Threshold decoding/validation is in `Control::Threshold` and
  `readUint16LE()`.
- Frequency choices are in `BUZZER_FREQUENCIES_HZ` and `Control::Tone`.
- Volume validation is in `Control::Volume`; duty conversion is in
  `updateBuzzer()`.
- Initial values and descriptions are created in `createGattServer()`.

If the tutor asks for a different tone, change the required entry in
`BUZZER_FREQUENCIES_HZ`, rebuild and upload. If the tutor asks for a different
payload format, change both its decoder/validation and the bytes placed in the
characteristic so its Read and Write representations remain consistent.

## Individual question 4: alarm decision and notifications

The complete decision is:

```text
alarm output ON = alarm enabled
                  AND range is valid
                  AND range < threshold
                  AND volume > 0
```

Suggested answer:

> Every 250 ms the main loop measures and filters the range. It updates the range
> characteristic and calls `notify()` when a client is connected. The phone only
> receives those packets after subscribing through descriptor `0x2902`. Notify
> is suitable because range is continuously changing and an individual dropped
> sample is not critical; another update follows 250 ms later. Four updates per
> second feel responsive while avoiding unnecessary BLE traffic. After measuring,
> the code compares the valid range with the threshold and updates the buzzer.

Important predictions:

- Enabled, range 49 cm, threshold 50 cm, volume above zero: **ON**.
- Enabled, range exactly 50 cm, threshold 50 cm: **OFF**, because the code uses
  strictly less than (`<`), not less than or equal.
- Enabled, range 65 cm, threshold changed from 50 to 70 cm: **ON**.
- Range moves from 65 to 75 cm with threshold 70 cm: **OFF**.
- Alarm disabled: **OFF**, regardless of range.
- Volume set to zero: no buzzer output.
- No valid sonar echo (`FFFF`): **OFF** for safety.

## Complete phone-to-hardware information flow

For a control write:

```text
nRF Connect on phone
  -> BLE Write request
  -> ATT/GATT characteristic on ESP32
  -> ControlCallbacks::onWrite()
  -> validate and store configuration
  -> updateBuzzer()
  -> LEDC hardware PWM
  -> passive buzzer
```

For a range notification:

```text
HC-SR04 trigger and echo
  -> pulseIn() measures round-trip time
  -> convert/filter into uint16 centimetres
  -> update range characteristic
  -> notify()
  -> BLE notification
  -> subscribed nRF Connect client
```

## Demonstration procedure

1. Open the serial monitor at 115200 baud and start nRF Connect.
2. Connect to `ESP32 Sonar Alarm` and expand the custom service.
3. Enable notifications on Range (`...0001`) and move an object to show changing
   phone values and serial measurements.
4. Write `01` to Enable (`...0002`).
5. Write `46 00` to Threshold (`...0003`) for 70 cm.
6. At a measured 65 cm, predict **ON**, then show that the buzzer sounds.
7. Move the object beyond 70 cm, predict **OFF**, and show that it stops.
8. Write `01`, `02` and `03` to Frequency (`...0004`) and demonstrate the three
   distinguishable tones while the object is inside the threshold.
9. Write `19`, `32` and `64` to Volume (`...0005`) to demonstrate 25%, 50% and
   100%. These are hexadecimal representations of decimal 25, 50 and 100.
10. Write `00` and then `01` to Enable to demonstrate remote disable/enable.

## Phone payload cheat sheet

Use hexadecimal/byte-array mode in nRF Connect, not text mode.

| Action | Characteristic | Hex payload |
|---|---|---|
| Disable alarm | Enable `...0002` | `00` |
| Enable alarm | Enable `...0002` | `01` |
| Threshold 50 cm | Threshold `...0003` | `32 00` |
| Threshold 70 cm | Threshold `...0003` | `46 00` |
| Threshold 100 cm | Threshold `...0003` | `64 00` |
| Select 800 Hz | Frequency `...0004` | `01` |
| Select 1600 Hz | Frequency `...0004` | `02` |
| Select 2400 Hz | Frequency `...0004` | `03` |
| Volume 25% | Volume `...0005` | `19` |
| Volume 50% | Volume `...0005` | `32` |
| Volume 100% | Volume `...0005` | `64` |

Example notification decoding: a range payload of `41 00` is `0x0041`, which is
decimal 65 cm.

## Required explanation: changes from the Week 7 BLE demo

Suggested answer:

> I retained the lecture demo's BLE server sequence: initialise BLE, create the
> server, create a service and characteristics, attach callbacks and descriptors,
> start the service, and advertise. I replaced its example UUIDs and single demo
> value with one custom sonar-alarm service containing five purpose-specific
> characteristics. Instead of assigning every property to one example value, I
> selected Read, Write and Notify according to each value's direction and use.
> I added four validated write callbacks, a `0x2902` subscription descriptor,
> `0x2904` format/unit descriptors and `0x2901` descriptions. The notification is
> now a filtered sonar measurement rather than an incrementing counter, and its
> rate is deliberately limited to 250 ms. Finally, I added the ultrasonic sensor,
> alarm comparison, three PWM frequencies, PWM volume control, invalid-echo
> handling, and advertising restart after disconnection.

Do not omit this comparison during assessment; the task specifically allocates a
direct one-point deduction if the changes from the lecture demo are not clearly
explained.
