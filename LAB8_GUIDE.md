# BLE-Controlled Sonar Alarm

This project makes the ESP32 the BLE **server**. A phone running a generic BLE
client such as nRF Connect discovers the server, reads/writes its configuration,
and subscribes to live range notifications.

## Exact wiring guide

The code uses GPIO 5 for TRIG, GPIO 18 for ECHO, and GPIO 25 for the passive
buzzer. All grounds must be connected together.

### 1. Power and trigger connections

| From | To | Resistor? |
|---|---|---|
| ESP32 `5V`/`VIN` pin | HC-SR04 `VCC` | None |
| ESP32 `GND` | HC-SR04 `GND` | None |
| ESP32 GPIO 5 | HC-SR04 `TRIG` | None |

Use the ESP32 board's 5 V pin only while it is powered by USB. Do not connect
HC-SR04 VCC to an unknown higher-voltage supply. TRIG is safe directly from the
ESP32 because it is a 3.3 V output and the HC-SR04 recognises it as high.

### 2. ECHO voltage divider - three 1 kOhm resistors

The HC-SR04 ECHO pin outputs about 5 V, but an ESP32 input is only 3.3 V safe.
Do **not** connect ECHO straight to GPIO 18. Make this divider:

```text
HC-SR04 ECHO
      |
    [1 kOhm] R1
      |
      +---------------- GPIO 18 on ESP32
      |
    [1 kOhm] R2
      |
    [1 kOhm] R3
      |
ESP32 GND
```

Put R2 and R3 in series: one leg of R2 connects to the junction/GPIO 18 row,
R2's other leg connects to one leg of R3, and R3's remaining leg goes to GND.
The resistors have no direction. This gives:

`5 V x (2 kOhm / (1 kOhm + 2 kOhm)) = about 3.33 V`

If you do not have three 1 kOhm resistors, three 10 kOhm resistors connected in
exactly the same pattern also produce 3.33 V. Prefer the 1 kOhm set if available.
Do not use 1 kOhm from ECHO and 10 kOhm to ground: that would still put about
4.55 V into the ESP32. Do not use the 220 Ohm resistors for this divider; three
of them have the right ratio but draw unnecessarily high current.

Common resistor colour codes are:

- 1 kOhm: brown-black-red (plus a tolerance band, often gold)
- 10 kOhm: brown-black-orange (plus tolerance)
- 220 Ohm: red-red-brown (plus tolerance)

### 3. Passive buzzer

For a small two-pin passive piezo buzzer:

```text
ESP32 GPIO 25 ---------------- buzzer +
ESP32 GND -------------------- buzzer -
```

No resistor is normally needed for a small passive piezo element. The longer
leg or a `+` marking identifies positive. If desired, a 220 Ohm resistor may be
placed in series between GPIO 25 and buzzer `+` for extra GPIO protection, but
it may reduce the volume.

This direct connection is only for a low-current piezo buzzer. If yours is a
magnetic buzzer or its rated current is above about 10-12 mA, use a transistor
driver instead of powering it from GPIO 25. The firmware requires a **passive**
buzzer; an active buzzer cannot reproduce the three selected frequencies.

### Final connection checklist

| Part pin | Connect to |
|---|---|
| HC-SR04 VCC | ESP32 5V/VIN |
| HC-SR04 GND | ESP32 GND |
| HC-SR04 TRIG | ESP32 GPIO 5 directly |
| HC-SR04 ECHO | R1, then the GPIO 18 divider junction |
| Divider lower end | ESP32 GND |
| Passive buzzer + | ESP32 GPIO 25 (optionally through 220 Ohm) |
| Passive buzzer - | ESP32 GND |

## GATT design

All related functions are grouped in one custom service:
`8f1d0000-6f7b-4a9e-9c2d-34a1b2c3d4e5`.

| UUID segment | Characteristic | Properties | Binary value |
|---:|---|---|---|
| `0001` | Range | Read, Notify | `uint16`, little-endian centimetres; `FFFF` = invalid/no echo |
| `0002` | Alarm enable | Read, Write | `uint8`: `00` off, `01` on |
| `0003` | Threshold | Read, Write | `uint16`, little-endian centimetres, 2-400 |
| `0004` | Tone choice | Read, Write | `uint8`: `01`=800 Hz, `02`=1600 Hz, `03`=2400 Hz |
| `0005` | Volume | Read, Write | `uint8`: 0-100 percent |

The range is **Read** so a client can request the current value and **Notify** so
the ESP32 can push a new value to a subscribed client every 250 ms. Notify is the
missing BLE property asked for in the task. It avoids repeated client polling and
does not require an acknowledgement for every live sample. Its Client
Characteristic Configuration Descriptor (`0x2902`) stores the subscription.

The four controls are **Read** so the phone can display the current state and
**Write** because the phone changes them. They do not need Notify because this
ESP32 is their only writer. Invalid lengths and out-of-range values are rejected.

Range and threshold both have a Characteristic Presentation Format descriptor
(`0x2904`): format = `uint16`, unit = metre (`0x2701`), exponent = -2. Therefore
one raw count is 0.01 m, or 1 cm. They also have readable user-description
descriptors (`0x2901`). If the design is changed to metres, both range and
threshold must use the same conversion. For example, 50 cm must become 0.50 m;
changing only the displayed range would make alarm comparisons wrong.

## Phone demonstration

1. Build/upload, open a 115200 baud monitor, and connect to **ESP32 Sonar Alarm**.
2. Open its custom service and enable notifications on Range (`...0001`).
3. Write hex `01` to Alarm enable (`...0002`).
4. Write hex `32 00` to Threshold (`...0003`) for 50 cm. Multibyte values are
   little-endian, so 100 cm would be `64 00`.
5. Write `01`, `02`, or `03` to Tone (`...0004`).
6. Write, for example, `19` (25%), `32` (50%), or `64` (100%) to Volume (`...0005`).
7. Move an object across 50 cm. Below 50 cm the passive buzzer sounds; at or above
   50 cm, on a timeout, or when disabled, it stops. Watch `uint16` range
   notifications change at the same time.

LEDC hardware PWM generates both the selected frequency and duty cycle. Volume
0-100% maps to 0-50% PWM duty because a passive buzzer's symmetric 50% square
wave is its strongest drive.

## Information flow to explain

A phone write travels through ATT/GATT to the matching characteristic. Its
`onWrite()` callback validates the exact binary payload and updates the ESP32
configuration. The main loop samples the ultrasonic echo, converts flight time
to centimetres, and tests:

`alarm enabled && valid range < threshold && volume > 0`

If true, LEDC outputs the selected frequency and volume duty to the buzzer;
otherwise duty is zero. After every 250 ms measurement the server updates the
range characteristic and calls `notify()`. The BLE stack sends it only when the
phone has enabled the `0x2902` subscription.

## Changes from the Week 7 BLE demo

The lecture demo's basic sequence is retained: initialise BLE, create a server,
create a service and characteristics, add descriptors, start the service, and
advertise. The lab implementation changes and extends it as follows:

- replaces the demo UUIDs/device name with one alarm service and five application
  characteristics instead of a single example value;
- assigns properties by purpose rather than giving every property to one value;
- adds four validated write callbacks that control real application state;
- adds the required `0x2902` subscription descriptor plus `0x2904` format/unit
  and `0x2901` human-readable descriptors;
- sends a filtered sonar `uint16` value instead of an incrementing demo counter;
- uses a deliberate 250 ms notification rate rather than a tight demo loop;
- drives the ultrasonic sensor and passive buzzer with application logic and
  LEDC hardware PWM; and
- restarts advertising after a client disconnects and safely turns the alarm off
  when no valid sonar echo exists.
