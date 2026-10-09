# ESP32 environmental monitor: lab guide

## 1. What is ready, and what you must do on hardware

The firmware, schema, runtime threshold controls and example queries are supplied.
You must wire the sensors, enter your own cloud configuration, upload to the ESP32,
and capture real Data Explorer screenshots. The project does not contain invented
measurements or assessment proof. The board setting is your existing
`upesy_wrover`; the pins below are for a classic ESP32 WROVER, not an ESP32-C3/S3.

There are two separate assessment preparations:

1. **Homework:** practise the lecture's temperature-to-InfluxDB chain and capture
   evidence with your student ID. Slide 45 requires each group to show proof
   within the first ten minutes of the next enrolled lab; it says failure to do
   so results in zero marks for that lab. Prepare the screenshot beforehand.
2. **Group lab:** add light sensing, demonstrate all four statuses, explain your
   own schema, use two metadata filters and two time ranges, and show physical
   changes in Data Explorer. Clearly explain your changes from the lecture demo;
   the task specifies a direct one-point deduction for omitting this explanation.

## 2. Components: use the resistors you already have

| Component | Quantity | Use |
| --- | --- | --- |
| ESP32 WROVER, USB data cable, breadboard, jumpers | One set | USB power and firmware upload |
| NTC thermistor | One | Your label confirms 10 kΩ; 25 C nominal reference and B = 3950 K remain assumed |
| Two-leg photoresistor (LDR) | One | Relative ambient light sensing |
| **Fixed 10 kΩ resistor** | **Two** | One in each sensor divider |

You do not need 220 Ω or 1 kΩ resistors, LEDs, a buzzer or a push button for this
lab. The thermistor itself is not one of the fixed resistors: you need a
thermistor plus two separate fixed 10 kΩ parts. Four-band 10 kΩ colours are
brown-black-orange, usually followed by gold tolerance. For comparison, 1 kΩ is
brown-black-red and 220 Ω is red-red-brown. Five-band colours differ; use kit
labels or a multimeter when unsure.

10 kΩ is a useful match for a 10 kΩ thermistor: at 25 C the divider is about
half of 3.3 V. It also gives a useful starting range for an LDR while keeping
current low. A 220 Ω or 1 kΩ part is not an equivalent drop-in replacement;
it changes the divider response. No resistor of any other value is required.

## 3. Wiring: exact pin-to-component guide

Disconnect USB before changing wiring. Use the ESP32's **GPIO labels**, not
physical header position numbers. If your board says `34` or `IO34`, that is
GPIO34. Use **3V3** for both dividers, never VIN/5V. Connect all grounds together.
Remove the previous buzzer/button/sonar circuit so this circuit is easy to check.

The two divider orientations are deliberately different:

```text
Temperature:
ESP32 3V3 ---- [fixed 10 kΩ] ----+---- [NTC thermistor] ---- ESP32 GND
                               |
                            GPIO34

Light:
ESP32 3V3 ---- [photoresistor] --+---- [fixed 10 kΩ] ------- ESP32 GND
                               |
                            GPIO35
```

| ESP32 pin | Connect it to | Exact purpose |
| --- | --- | --- |
| 3V3 | Breadboard positive rail | 3.3 V sensor supply |
| GND | Breadboard negative rail | Common reference/return |
| GPIO34 | Junction between first fixed 10 kΩ and thermistor | Temperature ADC1 input |
| GPIO35 | Junction between LDR and second fixed 10 kΩ | Light ADC1 input |
| USB | USB data cable to computer | Board power, upload and Serial Monitor |

| Component end | Connect to |
| --- | --- |
| Temperature fixed 10 kΩ, first end | 3V3 rail |
| Temperature fixed 10 kΩ, second end | Temperature junction, GPIO34 |
| Thermistor, first leg | Temperature junction, GPIO34 |
| Thermistor, second leg | GND rail |
| LDR, first leg | 3V3 rail |
| LDR, second leg | Light junction, GPIO35 |
| Light fixed 10 kΩ, first end | Light junction, GPIO35 |
| Light fixed 10 kΩ, second end | GND rail |

Both sensors and both fixed resistors have no polarity. Their **position in the
divider** matters. GPIO34 and GPIO35 are ADC1 input-only pins; these are suitable
for readings with Wi-Fi active. They have no internal pull resistors, so the two
external divider resistors are essential. Do not copy the lecture's
`PIN_ANALOG_IN 1` onto this classic ESP32: GPIO1 is its serial TX pin.

### Concrete breadboard example

Keep the ESP32 alongside the breadboard so its wide board does not cover holes.
On a standard board, a-e of each numbered row are connected; f-j are a separate
connected group. The centre trench separates those groups. Rails may be split
halfway; use one continuous rail section or bridge the split after checking it.

1. Run ESP32 3V3 to the red/+ rail and GND to the blue/- rail.
2. Put the temperature fixed 10 kΩ between **a5 and a10**.
3. Run **b5 to the 3V3 rail**.
4. Put the thermistor between **b10 and b15**; bend its leads gently or use
   extension jumpers if needed. Its legs must land on separate numbered rows.
5. Run **c10 to GPIO34** and **c15 to the GND rail**.
6. Put the LDR between **a20 and a25**. Run **b20 to the 3V3 rail**.
7. Put the second fixed 10 kΩ between **b25 and b30**.
8. Run **c25 to GPIO35** and **c30 to the GND rail**.
9. Check that the temperature node at row 10 and light node at row 25 are
   separate. Check that 3V3 and GND are not directly shorted.
10. Reconnect USB. If using a multimeter, room-temperature GPIO34 should be
    roughly 1.65 V for the assumed 10 kΩ NTC, not a guaranteed exact value.

The row numbers are an example; the electrical junctions above are the real
requirement. Do not put both sensor legs in the same connected five-hole group.

### Expected directions and sensor conversion

Warming an NTC reduces its resistance. In this temperature circuit, GPIO34
voltage **falls** while the computed temperature **rises**. The code averages
16 calibrated millivolt reads, then uses:

```text
R_NTC = 10000 × V_node / (V_supply - V_node)
T_K = 1 / (1 / 298.15 + ln(R_NTC / 10000) / 3950)
T_C = T_K - 273.15 + offset
```

`include/lab_config.h` holds the actual fixed resistance, nominal thermistor
resistance, B value, assumed supply (3300 mV) and temperature offset. Verify
your thermistor specification before claiming absolute accuracy. A room
thermometer can help choose a small offset; an offset cannot correct an
incorrect thermistor B value over a wide range. The firmware flags readings
outside its supported -10..80 C range or near the voltage rails as faults.

Your kit label says only **10 kΩ**. This establishes nominal resistance, but it
does not establish B. Leave B = 3950 K as the documented working assumption
unless you find a datasheet or model number. Check displayed room temperature
against a thermometer and explain the limitation in assessment; exact absolute
temperature calibration cannot be proved from the resistance label alone.

Bright light reduces LDR resistance. With the LDR on the upper supply side,
GPIO35 voltage and ADC reading **rise**. Covering it makes them **fall**.
`light_percent = 100 × averaged_raw_ADC / 4095`, using 12-bit resolution.
This is a **relative ADC full-scale percentage, not lux or a calibrated
percentage of physical illuminance**. LDR response is nonlinear; very bright
light can saturate the ADC. Cover and uncover it and choose a threshold between
your observed readings. A disconnected LDR can resemble darkness; this simple
two-wire circuit cannot reliably distinguish those conditions.

## 4. InfluxDB Cloud setup, following the lecture

Use the course's **InfluxDB Cloud v2 / Cloud (TSM)** account with buckets and
Flux Data Explorer, as shown on slides 37-44. InfluxDB 3 products have different
query interfaces; these Flux examples are for the lecture's v2/TSM workflow.
If your course account presents only SQL, resolve that account/product mismatch
with the tutor before switching away from the required lecture workflow.

1. Log in to your InfluxDB Cloud account.
2. Under **Load Data → Buckets**, create `environment_lab` (or use your own name).
   Choose a retention period long enough to keep homework plus lab evidence;
   30 days is suitable if offered. The slide's free-plan limits may differ from
   your current account, so use the options actually available to you.
3. Under **Load Data → API Tokens**, create a token for this ESP32 project.
   A custom token with write permission for this bucket is enough for uploads.
   The lecture demonstrates an all-access token; that also works, but this guide
   uses narrower permissions. Use the logged-in UI for reading/querying data.
4. Copy your **server base URL**, **organisation ID**, **bucket name** and token
   from your account/client-library setup. The URL must be HTTPS and must not
   include `/orgs/...` or `/api/v2/write`. Do not reuse the lecturer's URL,
   organisation or token blindly: use your account's endpoint and credentials.
5. Open existing `include/secrets.h`. Preserve the Wi-Fi definitions already
   there and add missing definitions shown in `include/secrets.h.example`.
   Only copy the example over if you do not already have a secrets file.
6. Set your unique `LAB_GROUP_ID` and actual `LAB_STUDENT_ID`. The latter is a
   tag in every point so you can display it in your homework screenshot.

```cpp
#define LAB_INFLUXDB_URL "https://YOUR_REGION.aws.cloud2.influxdata.com"
#define LAB_INFLUXDB_ORG "YOUR_ORG_ID"
#define LAB_INFLUXDB_BUCKET "environment_lab"
#define LAB_INFLUXDB_TOKEN "YOUR_BUCKET_WRITE_TOKEN"
#define LAB_STUDENT_ID "YOUR_STUDENT_ID"
```

Replace placeholders with real values. Keep the token and Wi-Fi password out of
screenshots and Git. `secrets.h` is ignored in both active and archived labs.
Incomplete configuration leaves local sensing running and disables cloud writes;
the build succeeding alone does not establish the cloud chain.

The root `platformio.ini` retains your board/platform and replaces MQTT/JSON
dependencies with the lecture's `tobiasschuerg/ESP8266 Influxdb`, pinned to
**3.13.2**. Despite its package name, it supports ESP32.

## 5. Build, upload and Serial Monitor

In the project terminal, use the PlatformIO VS Code buttons or:

```sh
pio run -e upesy_wrover
pio run -e upesy_wrover -t upload
pio device monitor -b 115200 --echo
```

If `pio` is not found, substitute `~/.platformio/penv/bin/pio`. Use a USB data
cable; charge-only cables do not upload. If multiple ports appear, use
`pio device list` and pass `--upload-port <port>` or monitor `--port <port>`.
If necessary, hold BOOT while the uploader displays `Connecting`, then release.

Expected output includes local sensor readings, your group/device/bucket label,
Wi-Fi connected, NTP waiting, a `Writing:` line, and **InfluxDB write OK**.
Wait at least 30 seconds for the first cloud point. The Serial Monitor has
newline-terminated commands; enter `help` to see their syntax. Commands are
local to the attached ESP32; no remote-control API or MQTT broker is involved.

## 6. Firmware mode and existing homework evidence

The firmware now always runs the full environmental monitor: both sensors are
sampled and points are written to `environment`. There is no homework-only
setting or temperature-only upload path.

Keep your completed homework screenshots. Previously uploaded
`lecture_temperature` points remain in InfluxDB until their retention period
expires; `queries/homework-temperature.flux` can still inspect that historical
data. New readings use `environment`.

## 7. Your own schema and justification

Bucket: `environment_lab`. Lab measurement: **`environment`**. One point joins
temperature, light, status and the thresholds used to classify that exact sample.
A single measurement keeps related readings aligned; two separate measurements
would require joining them to reconstruct the decision.

InfluxDB tags are indexed string metadata useful for filtering/grouping. Fields
are typed values suited to measurements, calculations and changing diagnostics.
Static/dynamic is a useful lecture guideline, not an absolute rule: query use
and the number of distinct tag combinations also matter.

| Element | Storage/type | Why and whether it changes |
| --- | --- | --- |
| `group_id` | Tag/string | Required identifying context; indexed group filter; fixed per build |
| `student_id` | Tag/string | Homework identity and optional student filter; bounded class cohort |
| `device_id` | Tag/string | ESP32 station MAC without colons; selects one physical device; fixed |
| `room` | Tag/string | Few locations, useful room filter; `lab_bench` default |
| `zone` | Tag/string | Few zones/floors, useful grouping; `ground` default |
| `device_model` | Tag/string | Few models, useful comparison; ESP32_WROVER default |
| `sensor_types` | Tag/string | Few sensor combinations; NTC10k_LDR for this monitor |
| `temperature_c` | Field/float | Compulsory measured quantity in Celsius; continuously changes |
| `light_percent` | Field/float | Compulsory relative light measurement; continuously changes |
| `environment_status` | Field/string | Compulsory classification; changes without splitting tag series |
| `status_code` | Field/integer | Numeric step plot: NORMAL=0, HOT=1, DARK=2, HOT & DARK=3, fault=-1 |
| `wifi_rssi_dbm` | Field/integer | Selected changing extra; numeric Wi-Fi strength diagnostic |
| `uptime_s` | Field/integer | Selected changing extra; increases while running, resets on reboot |
| `sample_interval_s` | Field/integer | Selected timing extra; 1 second in this build |
| `upload_interval_s` | Field/integer | Selected timing extra; 15-second minimum, plus request duration |
| `temperature_threshold_c` | Field/float | Selected changing extra; records live `hot` command value |
| `light_threshold_percent` | Field/float | Selected changing extra; records live `dark` command value |
| `firmware_version` | Field/string | Selected version extra; retained with sample; no version grouping needed |
| `thermistor_mv` | Field/integer | Divider diagnostic; helps check conversion and wiring |
| `light_adc_raw` | Field/integer | Raw averaged 0..4095 light value; helps choose threshold |
| `sensor_ok` | Field/boolean | False if temperature conversion failed; avoids claiming a normal reading |
| Timestamp | Point timestamp | NTP-synchronised UTC Unix epoch, transmitted in milliseconds |

The design exceeds the four-extra requirement. Even just `room`, `device_id`,
`wifi_rssi_dbm` and `uptime_s` satisfy it, with RSSI and uptime changing during
operation. Both thresholds also change live through serial commands.

Device identifiers do have distinct values per board, but there are only a few
boards in a group/class and device filtering is central. This bounded use is
justified; an unbounded per-reading ID, timestamp, uptime or RSSI as a tag would
continually create new series and should be avoided in this v2/TSM design.

### If the tutor asks to swap a tag and a field

- **RSSI as a tag?** It is technically possible to encode it as a string, but
  it creates extra tag combinations as signal strength changes and makes
  numerical analysis awkward. A numeric field is a better fit.
- **Device ID as a field?** Possible if rarely queried, but you lose its indexed
  filter/grouping role and make per-device queries less convenient. A tag is
  useful for the bounded number of actual boards here.
- **Status as a tag?** A defensible alternative: only four normal states (plus
  fault) and convenient indexed state filters. It would split each device's
  data into status-based series. Here a string field and numeric code preserve
  one identity tag set and allow status history via field selection.
- **Room can change, so must it be a field?** No. A small set of room values is
  still suitable indexed metadata. If a device moves, future points should have
  its new room tag; past points retain the old room. Current room/zone constants
  require re-upload to change; they are not counted as runtime-changing extras.
- **Firmware version as a tag?** Appropriate if comparing releases is a frequent
  query and versions stay bounded. A field is adequate for this diagnostic use.

Never silently change an existing field from float to string/integer in the same
measurement; InfluxDB can reject conflicting field types. Use a new field name
or plan a deliberate migration instead. Moving tags/fields does not rewrite
historical points or magically update existing queries.

## 8. Timestamps, upload rate and data flow

The code samples once per second, averaging 16 ADC reads per sensor. The sampling
loop has no HTTP requests; a FreeRTOS network task owns Wi-Fi and the InfluxDB
client. A one-element queue contains the latest reading. Every 15 seconds or
slower, the network task uploads the latest completed sample, not all 15 samples.
This is at most four points per minute (240/hour, 5760/day), with the related
fields sent in one request. Changes do not trigger extra immediate uploads.

The lecture uploads about every second using `delay(1000)`; this lab deliberately
reduces cloud traffic while remaining responsive for slow environmental changes.
Hold each demo condition for **45-60 seconds** so several uploads capture it.
Very short changes between uploads may not appear in the bucket. Failed/offline
samples are not saved persistently or replayed; expect gaps after an outage.
The client retry buffer is disabled and attempts are paced by the firmware to
avoid bursts. Offline local sampling/classification continues.

NTP is started with the lecture's Sydney timezone rule. No point is uploaded
until an NTP reply has arrived this boot and the clock has a plausible epoch.
`gettimeofday()` captures
the **sample time** after the ADC reads, and `Point::setTime(epochMs)` plus
`WritePrecision::MS` sends that explicit UTC time. `millis()` schedules work;
it is not a database timestamp. `uptime_s` comes from the ESP32 monotonic timer,
not its wall clock. The cloud stores an instant; Data Explorer may display it in
UTC or your browser's timezone. Do not add ten/eleven hours to epoch timestamps.
NTP synchronises an ordinary classroom clock; millisecond representation does
not claim millisecond absolute accuracy. DNS/NTP access must work on your Wi-Fi.

```text
Temperature/light change physically
  -> sensor resistance changes
  -> 3.3 V divider node voltage changes
  -> ESP32 ADC1 measures GPIO34/GPIO35
  -> average, NTC conversion / relative light scaling
  -> classify with the sample's temperature and light thresholds
  -> attach sample UTC timestamp and metadata in a Point
  -> client serialises InfluxDB line protocol
  -> HTTPS POST /api/v2/write with org, bucket, precision=ms and token
  -> InfluxDB authenticates token and stores point in bucket
  -> Data Explorer queries by measurement/tags/fields/time range
  -> graphs/table show the measurements and status
```

### Explain one reading as one data point

Illustrative point only, **not real evidence**; the full firmware adds diagnostics:

```text
environment,group_id=group01,device_id=AABBCCDDEEFF,room=lab_bench temperature_c=31.20,light_percent=12.00,environment_status="HOT & DARK",status_code=3i,temperature_threshold_c=30.00,light_threshold_percent=25.00 1791414000000
```

The measurement is `environment`; the comma-separated indexed strings before
the first space are tags; the values between spaces are fields. `3i` denotes
an integer, quoted text is a string and unquoted decimal readings are floats.
The final integer is a UTC epoch millisecond timestamp because the API request
uses `precision=ms`. Every field in that point shares the identity tags and
timestamp. The Arduino Point library performs escaping; do not hand-concatenate
unescaped labels into line protocol. The tag set plus measurement and timestamp
identifies a point; fields written later at that same identity/time can merge
or overwrite, so distinct sample times matter.

## 9. Status decisions and live threshold assessment

Defaults: `HOT_C = 30.0` and `DARK_PERCENT = 25.0`.

| Temperature condition | Light condition | Status |
| --- | --- | --- |
| T ≤ hot threshold | Light ≥ dark threshold | NORMAL |
| T > hot threshold | Light ≥ dark threshold | HOT |
| T ≤ hot threshold | Light < dark threshold | DARK |
| T > hot threshold | Light < dark threshold | HOT & DARK |

Exactly 30 C is not HOT at the default threshold. Exactly 25% is not DARK.
Readings and threshold commands are rounded to two decimal places before
classification, matching the values stored in InfluxDB. This avoids a rounded
reading appearing equal to a threshold while an unseen extra decimal determines
the opposite status. Use raw sample details for boundary questions, rather than
an averaged graph window.
Here NORMAL means neither selected alarm condition; it does not assert a
comfortable lower temperature or an upper light limit. No hysteresis is used;
values near a threshold can alternate status. The averaged readings reduce noise.
Invalid temperature gives SENSOR_FAULT, status_code=-1 and sensor_ok=false;
temperature_c is omitted rather than uploading NaN/fabricated temperature.

In Serial Monitor, send a newline after:

```text
hot 28
dark 15
help
```

Changes apply at the next sample, are uploaded with that sample's status and
persist until reboot. Reboot restores constants from `include/lab_config.h`.
Allowed ranges are -10..80 C and 0..100%; malformed commands/NaN are rejected.

Practise with a reading of **29 C and 20%**:

1. Thresholds 30 C and 25% → **DARK**.
2. Change only hot threshold to 28 C → **HOT & DARK**.
3. Then change dark threshold to 15% → **HOT**.
4. Set hot threshold to exactly 29 C, dark still 15% → **NORMAL**, if the next
   actual measurement is exactly 29 C. Explain that live sensor noise can move it.

Predict from the currently measured numbers before sending a tutor-requested
change; then show the Serial output and the next cloud points. If fingers cannot
raise your thermistor above 30 C, select a reachable threshold slightly above
room temperature and below the finger-warmed reading, and explain that choice.

## 10. Data Explorer: recent, historical and metadata queries

First use the lecture's visual builder: select bucket, time range,
measurement, tag filters and field, then run/submit. UI wording can vary.
For reproducibility, the `queries/` folder also supplies Flux scripts. Replace
`YOUR_GROUP_ID`, `YOUR_DEVICE_ID` and bucket name in those scripts. Device ID is
printed at boot and in every `Writing:` line; it is the MAC without colons.

### Recent temperature and light plots

1. Select **Last 15 minutes**, bucket `environment_lab`, measurement `environment`.
2. Filter **group_id** to your group and **device_id** to your actual ESP32.
   These are two selected schema elements used to distinguish data. Add room or
   zone if useful. One board is sufficient; do not invent another device's data.
3. Select `temperature_c`, run and choose Graph. Show Celsius on its axis.
4. Select `light_percent`, run and choose Graph. Show relative percent on its axis.
5. Save separate screenshots. A combined plot is also possible, but the two
   quantities have different units; two charts are easier to explain.
6. `queries/recent-trend.flux` reproduces either graph with a `field` variable.
   It uses 15-second means; for the live demonstration, raw readings without
   aggregation are also appropriate.

### Inspect environmental status

Select only `environment_status`, disable numerical mean aggregation and use
Table/Raw Data: strings cannot be averaged. `queries/status-history.flux` shows
the stored status, timestamp and identity. `queries/status-changes.flux` keeps
the first numeric status and subsequent recorded changes, using `status_code`.
If graphing the code, select step interpolation when available; averaging status
codes produces meaningless fractional states. The codes are categories, not a
severity scale. For temperatures, light and thresholds together, use
`queries/sample-details.flux` and choose Table.

### Demonstrate a second range with historical measurements

1. Let the device run long enough to collect older readings. Homework can be
   collected in advance; lab historical readings must be in `environment`.
2. Change from Last 15 minutes to **Last 24 hours** using the UI range selector.
   `queries/historical-trend.flux` gives 1-minute means for that wider graph.
3. For a demonstrably older, non-overlapping interval, use a custom range ending
   15 minutes ago, or change the script to `range(start: -24h, stop: -15m)`.
   This is empty if the device has not yet collected data older than 15 minutes.
4. Explain: increasing the time range includes older points; averaging more
   points into one-minute windows smooths detail. It does not change stored data.

For a grouping exercise, remove the single-device filter and group by
`device_id` and `_field`, retaining `group_id` filtering. The historical script
does this. With two real boards, it creates separate curves; with one, it
correctly shows one device. Change `room` or `zone` filters if the tutor asks.
Selecting a tag value that has no readings in the chosen range returns no data.

If builder selections and script disagree (slide 44), return to Builder and
regenerate by toggling the field/measurement selection, or paste a complete
known-good Flux script into Script Editor. Do not mix incomplete SQL and Flux.

## 11. Physical demonstration and evidence checklist

Tune thresholds using actual uncovered/covered light values and room/finger-warmed
temperatures. Write down your chosen thresholds and why they are reachable.
Use Last 15 minutes with group/device filters and refresh the graphs/table.

| Action, held for 45-60 seconds | Expected when thresholds are reachable | Evidence |
| --- | --- | --- |
| Uncovered LDR, room-temperature NTC | NORMAL | Baseline temperature/light and status |
| Warm NTC gently with fingers, LDR uncovered | HOT | Temperature increases past threshold |
| Let NTC cool, cover LDR | DARK | Light drops below threshold |
| Keep LDR covered, warm NTC | HOT & DARK | Both changes in same point |
| Uncover LDR while NTC stays warm | HOT | Light recovers, temperature still hot |
| Release NTC and allow cooling | NORMAL | Recovery below hot threshold |

The cooling stage can take longer than one minute. Follow actual readings rather
than assuming the sensor cooled. Hold only the thermistor body; do not tug leads
or touch exposed wiring so the contact does not destabilise connections.

Prepare genuine evidence of:

- Homework graph plus student-ID table, ready before the session.
- Wiring photo showing GPIO34, GPIO35, 3V3, GND and two fixed 10 kΩ resistors.
- Recent temperature and light graphs with group/device filters visible.
- A raw status table or categorical code plot showing recorded transitions.
- Historical graph with a wider/different time range and older timestamps.
- A live threshold change with prediction, serial result and uploaded threshold.
- Your own spoken explanation of the schema and lecture-demo changes below.

## 12. Required explanation: changes from the lecture demo

| Lecture extract | This implementation and reason |
| --- | --- |
| Slides 38-40: cloud client, CA certificate, Point and connection validation | Retained the same APIs and library 3.13.2; direct HTTPS to the v2 bucket |
| Demo measurement `kaisesp32`, field `temp`; `get_temp()` left for students | Defined `environment` and `temperature_c`, implemented the NTC equation |
| One thermistor, analog input defined as 1, resolution 11 bits | Two sensors on classic ESP32 ADC1 GPIO34/35; 12-bit light reads and calibrated thermistor millivolts |
| Tags `device`, `location`, `esp32_id` using `WiFi.BSSIDstr()` | Added unique group/student tags, room/zone/model/sensor context; use ESP32 station MAC because BSSID identifies the router/AP |
| Synchronises Sydney time, writes without an explicit timestamp in the loop | Retained NTP/TZ principle; explicitly send sample UTC epoch milliseconds and skip untimed readings |
| `clearFields()`, `addField()` and `writePoint()` in the sensing loop | Retained those operations; one multi-field sample adds light, status, diagnostics and the thresholds used |
| `delay(1000)` between writes, blocking Wi-Fi connection in setup | Local sensing at 1 Hz, separate network task, cloud updates every 15 seconds or slower, paced reconnects |
| Temperature-only data, no environmental classification | Added strict HOT/DARK comparisons, combined state, string status plus numeric code and invalid-temperature state |
| Credentials embedded in main.cpp; all-access token example | Local ignored secrets header, per-account endpoint and bucket-write token; no credentials printed |
| Basic bucket/measurement selection in Data Explorer | Added group/device filtering, optional room/zone grouping, separate charts, status table and recent/historical queries |

Suggested explanation in your own words:

> I kept the lecture's InfluxDBClient with the Cloud CA certificate, Point tags,
> clearFields/addField/writePoint sequence, connection validation and NTP time
> synchronisation. I implemented the missing thermistor conversion and added a
> photoresistor on a second ADC1 pin. Each environment point now contains both
> readings, their status and the actual thresholds, with group and board identity
> tags. I use the board's station MAC instead of the access point's BSSID. The
> readings are sampled each second but uploaded every fifteen seconds or slower
> by a separate network task, with an explicit UTC sample timestamp. I added live
> threshold commands, fault handling and Data Explorer queries for status changes,
> metadata filters and two time ranges.

Be ready to point to the corresponding code rather than just memorising this.
The slides' prose calls writing functions Point functions; technically
`sensor.addField()` is a Point method and `client.writePoint(sensor)` is an
InfluxDBClient method. The timestamp-only auto-arrival design in slide 36 can
be acceptable for immediate writes, but sample timestamps more clearly identify
when the reading occurred if a network request is delayed.

## 13. Individual assessment practice

1. **Explain schema:** name `environment`, identify tags/fields from section 7,
   explain UTC timestamps and justify tag/field alternatives for a nominated
   element. Say why related values share one point.
2. **Trace a reading:** start at the physical resistance change, identify the ADC
   node, conversion and classification, then point to measurement, tags, fields
   and epoch timestamp in a real Serial `Writing:` line.
3. **Modify a threshold:** use your current readings to predict HOT/DARK first,
   send the requested `hot` or `dark` command, and verify the new status and
   uploaded threshold. Explain strict boundary comparisons and reset on reboot.
4. **Change Explorer selection:** locate your group/device, choose another
   field/tag or range requested by the tutor, run again and explain why the
   displayed curves/tables or number of points changed.

## 14. Troubleshooting and local verification

| Symptom | Check |
| --- | --- |
| Cloud disabled message | Add all missing settings, unique group/student IDs; rebuild/upload |
| Wi-Fi retries | 2.4 GHz SSID/password; hotspot/router Internet; no captive portal |
| NTP waiting / UTC_ms=0 | DNS and UDP NTP access; try allowed NTP servers on your network |
| Validation/write 401 or 403 | Token validity and bucket write permission; exact org; regenerate token if needed |
| Bucket not found / 404 | Exact bucket name, org ID and your account's server URL |
| TLS/certificate error | Clock, correct HTTPS endpoint, trusted CA compatibility; retain TLS validation |
| HTTP 429 | Quota/limits; increase UPLOAD_MS rather than rapid retries |
| Write OK but empty Explorer | Correct bucket/measurement/group/device/field; range includes sample UTC; run/refresh |
| Thermistor NaN/fault | Check row10 junction, 3V3/GND orientation, resistor/NTC constants and rail shorts |
| Temperature falls when warmed | Thermistor divider likely reversed; use the exact temperature orientation |
| Light rises when covered | LDR divider reversed; use supply → LDR → node → fixed resistor → GND |
| Light stuck near 0 or 100 | Rail/junction connection, blocked light, ADC saturation; examine raw readings |
| String mean/type error | Select status alone as a table; numeric aggregation only for numeric fields |
| No historical data | Collect real points earlier, widen range within retention; do not manufacture timestamps |

Host verification (no board needed):

```sh
c++ -std=c++11 -Wall -Wextra -Werror -Iinclude tests/environment_logic_test.cpp -o /tmp/environment_logic_test
/tmp/environment_logic_test
pio run -e upesy_wrover
```

Tests verify the four statuses, exact boundaries, tutor threshold predictions,
invalid readings, thermistor conversion direction and light scaling. Compilation
checks the actual ESP32 framework and lecture library APIs. Neither substitutes
for wiring, Wi-Fi/cloud connectivity or real assessment evidence.

## References

Primary course source: the attached **W9. IoT Cloud 2026.pdf**, printed slides
35-36 (schema), 37-44 (demo/setup/explorer) and 45 (homework).

- [Arduino InfluxDB client](https://github.com/tobiasschuerg/InfluxDB-Client-for-Arduino): cloud CA, Point APIs and timestamp precision.
- [InfluxDB Cloud v2 bucket setup](https://docs.influxdata.com/influxdb/cloud/admin/buckets/create-bucket/): bucket creation and retention.
- [InfluxDB schema guidance](https://docs.influxdata.com/influxdb/cloud/write-data/best-practices/schema-design/): indexed tags, fields and cardinality.
- [Data Explorer](https://docs.influxdata.com/influxdb/cloud/visualize-data/explore-metrics/): builder, filters and visualisation selection.
- [Flux query guide](https://docs.influxdata.com/influxdb/cloud/query-data/flux/): time ranges, filtering and grouping.
- [Espressif ADC API](https://docs.espressif.com/projects/arduino-esp32/en/latest/api/adc.html): raw/millivolt readings and attenuation. This repo pins Arduino ESP32 2.0.17.
