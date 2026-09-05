# esp-multi Bug Workbook

Tracked issues from code review. Work through them top-down — critical bugs first,
then significant bugs, then design flaws / minor issues.

**Second review pass: 2026-07-16.** All previously fixed items were re-verified and are
archived in the table below. Two old fixes turned out to be incomplete (B4, D1) and are
reopened as B4 and M3. New findings are numbered continuing the old scheme
(C7+, B7+, M1+). Behavioral quirks that are not bugs are collected under
[Caveats](#caveats-behavioral-quirks-not-bugs).

---

## ✅ Fixed (archive)

| ID | Title | Resolution |
| --- | --- | --- |
| C1 | `setConfig()` self-assignment no-op | commit `ef4d784` |
| C2 | `Sensor::Offset` never set from constructor param | commit `8a06fcf` |
| C3 | `SensorType` enum duplicate values (`sgp30`/`mhz19`) | commit `6c4ffdb` |
| C4 | `new std::atomic<bool>()` assigned to non-pointer atomics | fixed |
| C5 | Double `request->send()` on `/restart` | fixed |
| C6 | Real WiFi credentials in source | fixed — credentials removed; verified via `git log -S` that they were **never committed**, so no history purge/rotation needed |
| B1 | ISR-shared button variables not `volatile` | fixed |
| B2 | Campfire brightness almost always increases | fixed |
| B3 | `webPage` leaked on every `configureDevice()` | fixed |
| B5a | Unreachable `return true` in `ts_http.cpp postData()` | fixed (restructured); the `logging.cpp` twin is still open, see B5 below |
| B5 | Unreachable `return true` in `logging.cpp postData()` | fixed 2026-07-16 (restructured together with B15) |
| B6 | Sunrise animation completed at halfway point | fixed |
| C7 | Data race: async web handlers mutate global config | fixed 2026-07-16 — handlers now stage a validated copy via `configman::stageConfig()` (atomic pointer handoff); `loop()` applies + persists it in `checkWebpageTriggers()` via `applyStagedConfig()` before acting on the restart flag. Also removed the hidden `saveConfig()`/recursion inside `deserializeConfig()` (ran on the async task too). `writeConfig()`/`m_ConfigChanged` flag removed. Remaining narrower race: `GET /api/config` still *reads* the config from the async task while `loop()` may be applying an update. |
| C8 | `sendStateTopic()` null-deref crashes offline/AP devices | fixed 2026-07-16 — nullptr guard added |
| C9 | SCD30 init timeout loop never breaks | fixed 2026-07-16 — returns on timeout, uses `millis() - start >= N` form |
| B7 | Invalid `static_cast` to `CTimeseriesHttp` with MQTT backend | fixed 2026-07-16 — guarded with `!UseMQTT` |
| B9 | Web triggers dead while unconfigured | fixed 2026-07-16 — `checkWebpageTriggers()` moved above the `!IsConfigured` return |
| B10 | `DHTSensor::init()` returns `true` on no response | fixed 2026-07-16 — returns `false` |
| B15 | Any HTTP status treated as success (buffered data destroyed) | fixed 2026-07-16 — `ts_http.cpp` and `logging.cpp` now require 2xx |
| D1 | Definitions in headers (ODR) — `events.h`, `button_inputs.h`, `handle_buttons.h` | fixed for those files; sweep was incomplete, see M3 |
| D4 | `getConfig()` deep-copies on every call | commit `5dc0bbd` |

---

## 🔴 Critical Bugs

*(none open — C7/C8/C9 fixed, see archive)*

---

## 🟠 Significant Bugs

---

### B4 — `millis()` overflow comparisons (REOPENED — fix incomplete)

- **Status:** [ ] reopened

The additive-deadline pattern (`deadline = millis() + N; if (millis() >/< deadline)`)
survives throughout the LED/button/time code. After the ~49.7-day wrap, buttons can go dead
for weeks, a sunrise alarm ends instantly or never, and NTP sync is silently skipped.
This matters: the device is an always-on lamp/alarm expected to run for months.

Remaining sites (all need the `millis() - start >= N` form):

- `src/led/button_inputs.cpp` L12, L22 (debounce)
- `src/led/ledstrip.cpp` L164–166 (pulse), L278/L283/L363 (`m_NextLEDActionTime`)
- `src/led/sunrise_alarm.cpp` L54, L78 (`m_AlarmEndTime`)
- `src/handle_buttons.cpp` L37, L77 (`buttonResetTime`)
- `src/timehelper.cpp` L25/L31, L37/L45 (NTP sync timeouts)
- ~~`src/sensors/sensors.cpp` L833/L837 (SCD30 timeout)~~ fixed with C9

---

### B8 — First-measurement priming subtracts seconds from a milliseconds clock

- **File:** `src/main.cpp` L453
- **Status:** [ ] open

`lastUpdate = millis() - configman::getConfig().MeasureInterval;` rewinds by
`MeasureInterval` **ms** (30 ms), but the gate compares against `MeasureInterval * 1000`.
The intended immediate first measurement never happens — a deep-sleep battery node sits
fully awake with WiFi on for a whole interval every wake cycle before measuring.
Fix: `lastUpdate = millis() - (unsigned long)MeasureInterval * 1000;`

---

### B11 — BME280 pressure published in Pa but labeled "mbar" (100× unit error)

- **File:** `src/sensors/sensors.cpp` L391, L410–413
- **Status:** [ ] open

`Adafruit_BME280::readPressure()` returns **pascals** (verified in the vendored library),
so a BME280 device reports ~101300 "mbar" instead of ~1013. The BMP280 path uses the
unified-sensor event which *is* hPa — the same "Pressure" series is off by 100× depending
on the attached chip. Fix: divide by 100.0 when storing and adjust the `pressure < 100`
no-connection guard accordingly.

---

### B12 — `m_Description` grows without bound across sensor re-scans

- **File:** `src/sensors/sensors.cpp` L71–72, L151
- **Status:** [ ] open

`sensorsInit()` clears `m_SensorTypes`/`m_SensorNames` but not `m_Description`. With no
sensors attached, `loop()` re-calls `sensorsInit()` every interval and each pass appends
`"No Sensors;"` — the String grows indefinitely (heap exhaustion on small configs, garbage
device description). Fix: clear `m_Description` alongside the other two.

---

### B13 — DS18B20 disconnect sentinel (−127 °C) reported as a valid temperature

- **File:** `src/sensors/one_wire.h` L50–53
- **Status:** [ ] open

`getTempC()` returns `DEVICE_DISCONNECTED_C` (−127.0) on failure; there is no check, so
−127 is marked `isValid = true`, sent to the timeseries, and drives the LED temperature
color. Fix: filter `tempC == DEVICE_DISCONNECTED_C` (or `< -55`).

---

### B14 — `Serial.print` and non-IRAM calls inside rain/wind GPIO ISRs; shared counters not `volatile`

- **Files:** `src/sensors/watersensor.cpp` L11, L23–37; `src/sensors/windsensor.cpp` L11–18, L46–72
- **Status:** [ ] open

(a) `Serial.print` from an ISR is unsafe on ESP32 (locks/allocations; a click arriving
during a flash write — e.g. config save — causes "Cache disabled but cached memory region
accessed" panics). Helper functions called from the ISR (`getFactor()`) are not
`IRAM_ATTR`. (b) `clickCounter`, `lastClickTime`, `peakSpeed`, … are ISR/main-loop shared
without `volatile`/atomics. Fix: move prints out of the ISRs, mark shared state
`volatile`, mark ISR-called helpers `IRAM_ATTR`.

---

### B16 — Unbounded `m_Data` buffer growth while the server is unreachable

- **Files:** `src/timeseries/timeseries.h` L30–36; `src/timeseries/ts_http.cpp` L51–74
- **Status:** [ ] open

On send failure the buffer is kept and grows every interval with no cap; each retry also
serializes the entire ever-growing buffer into a `JsonDocument` *and* a `String` (double
footprint). A prolonged outage ends in heap exhaustion. Fix: cap `m_DataSeries` size with
oldest-first eviction.

---

### B17 — Uninitialized `time_t now` read in `initTime()` wait loop (UB)

- **File:** `src/timehelper.cpp` L38–44
- **Status:** [ ] open

`now` is tested (and printed) before `time(&now)` is ever called. If stack garbage lands
in the valid range, the wait is skipped and the first samples get 1970-epoch/garbage
timestamps. Fix: `time_t now = 0; time(&now);` before the loop; also don't `delay(500)`
before the first check.

---

### B18 — `campfireMode()` out-of-bounds read on strips with more than 256 LEDs

- **File:** `src/led/ledstrip.cpp` L292–298, L327–358
- **Status:** [ ] open

The template-growth check truncates the loop index: `if (colorTemplate.size() <= uint8_t(i))`
— growth stops at 256 entries, but `LastIndexRed/Green` are clamped to `m_NrOfPixels - 1`
and used as indices. On a 300-LED strip the random walk eventually indexes past the end of
the vector (UB). Same block: `i <= m_NrOfPixels` off-by-one,
`emplace_back(colorTemplate[i % 30])` passes a reference into a vector that may reallocate
mid-call, and the template is rebuilt on the heap every 10 ms tick. Fix: drop the `uint8_t`
cast, build the template once.

---

### B19 — Sunrise mode via `/api/led` never ramps (`m_SunriseStartTime` uninitialized)

- **Files:** `src/led/ledstrip.cpp` L3–19, L50–53, L190–212; `src/led/leds_service.cpp` L72–75
- **Status:** [ ] open

`m_SunriseStartTime` is only set by `CSunriseAlarm::startSunrise()`. A `POST /api/led`
with `Mode: 4` reads the uninitialized member (UB) — `timeFactor` clamps to max and the
strip jumps straight to "risen". Adjacent: `apply()` sets the requested brightness, then
`applyModeAndColor()` overwrites `m_Factor = 0`; and if `AlarmSettings.IsActivated`, the
main loop never calls `runModeAction()`, so an API sunrise doesn't animate at all.
Fix: set `m_SunriseStartTime = millis()` in the sunrise case and initialize it in the
constructor.

---

### B20 — MQTT `set`/`switch` topics are never parsed but replay stale LED state

- **File:** `src/mqtt_events.cpp` L39–45, L111–128
- **Status:** [ ] open

`subscribe()` subscribes to three topics but `onMqttMessage()` only parses
`/json/set`; the other two just set `wasStriggered = true`, and `handleMQTT()` then applies
the *previous cached* color/on/brightness — right after boot that's `{0,0,0}`/off, so any
publish to `…switch` forces the lamp off/black and pushes bogus state to Home Assistant.
Also off-by-one: `topic.indexOf("/json/set") > 0` should be `>= 0` — with an empty
`MQTTTopic` JSON commands are silently ignored. Fix: implement handlers for `set`/`switch`
or don't subscribe to them; fix the `indexOf` comparison.

---

### B21 — MQTT brightness not range-validated → `m_Factor > 1` → out-of-range double→uint8_t (UB)

- **Files:** `src/mqtt_events.cpp` L135–139; `src/led/ledstrip.cpp` L86–90
- **Status:** [ ] open

The HTTP API clamps brightness to 0–100, the MQTT path takes it raw. Home Assistant
defaults to 0–255, so brightness 255 → `m_Factor = 2.55` →
`m_Pixels.Color(255 * 2.55, …)` — converting an out-of-range double to `uint8_t` is UB
(garbage channel values). Negative values likewise. Fix: clamp in `onMqttMessage()`.

---

### B22 — SCD30 forced recalibration in the read path (sensor NVM wear + ratcheting bias)

- **File:** `src/sensors/sensors.cpp` L602–609, L830
- **Status:** [ ] open

Every reading below 400 ppm issues `setForcedRecalibrationFactor(400 + (420 - CO2))` —
FRC persists to the sensor's non-volatile memory, so an outdoor sensor near 400 ppm gets
its calibration rewritten every cycle, each correction overshooting by 20 ppm. The
boot-time `setForcedRecalibrationFactor(420)` assumes outdoor air on *every* boot: an
indoor reboot at 900 ppm miscalibrates by ~480 ppm. Fix: remove FRC from the read path;
make recalibration an explicit user action.

---

### B23 — SHT30 raw-I2C read ignores `requestFrom`/`read` results — garbage humidity marked valid

- **File:** `src/sensors/sensors.cpp` L461–484
- **Status:** [ ] open

If fewer than 6 bytes arrive, `Wire.read()` returns −1 → `0xFFFFFFFF` in the unsigned
buffer → astronomically wrong humidity flagged `isValid = true`. The `available() != 0`
check catches *extra* bytes, not missing ones. Fix: check `requestFrom(...) == 6`.

---

### B24 — SHT30-only device never produces values

- **File:** `src/sensors/sensors.cpp` L791–796 (detection), `getValues()` (no `sht30` branch)
- **Status:** [ ] open

`initI2CSensor` registers `SensorType::sht30`, which makes `sensorsInit()` return true and
stops re-scans — but `getValues()` has no `sht30` case; the SHT30 is only read as a side
effect inside `getEnv()`, which runs only when a BMP280 is *also* present. An SHT30-only
device logs "No Values" forever. Its names are also never added to `m_SensorNames`, so
`initDevice` won't declare it. Fix: add a dedicated `sht30` read path.

---

## 🟡 Design Flaws (previous pass, still open)

---

### D2 — No authentication on the configuration web API

- **File:** `src/webpage.cpp`
- **Status:** [ ] open — *worse than originally noted*

`POST/PUT /api/config` accept arbitrary JSON from any client and every endpoint sends
`Access-Control-Allow-Origin: *`. That means not only LAN clients but **any web page open
in a browser on the same LAN** can read the WiFi password (`GET /api/config`) or
reconfigure/restart the device (browser-driven CSRF). Fix as originally planned
(pre-shared token in `X-Authorization`, HTTP 401 otherwise) *and* drop the wildcard CORS
on mutating endpoints.

---

### D3 — MQTT timeseries ignores `BufferedValues` — and silently drops values when disconnected

- **File:** `src/timeseries/ts_mqtt.cpp`
- **Status:** [ ] open

Confirmed still open: `newValue()` publishes immediately, and when the broker is not
connected it just logs "MQTT: Not connected" and **drops the value** (L28–32) — no
buffering at all, unlike the HTTP backend. Fix options unchanged: implement batching in a
proper `sendData()` override, or document the difference and at least buffer while
disconnected.

---

### D5 — WiFi password printed to Serial in unconfigured state

- **File:** `src/main.cpp` L604–605, L616–617
- **Status:** [ ] open (nuanced)

Still present. Nuance from re-review: inside the `isAccessPoint` block the print shows the
**AP credentials** the user needs to join the setup network — arguably intentional.
However `serializeConfig(&config)` at L616–617 dumps the *entire* config including the
password on every 15 s iteration regardless. Keep the AP-credentials hint if desired, drop
the full config dump (or redact the password in `serializeConfig`).

---

### D6 — Hard `ESP.restart()` on WiFi timeout instead of graceful fallback

- **File:** `src/main.cpp` L112–115
- **Status:** [ ] open

Confirmed still present: 30 s timeout → unconditional `ESP.restart()` → endless reboot
loop when the router is down, never reaching the AP fallback in `setup()`. Fix unchanged:
`return false` and let the caller's `while (!tryConnect(...))` / `createAccesPoint()`
logic handle it.

---

## 🟢 Minor Issues (second pass)

| ID | File / lines | Issue |
| --- | --- | --- |
| M1 | `src/config.cpp` L574–580 (×7 days) | `deserializeSunrise` missing-day guard is dead: the default assignment is unconditionally overwritten on the next line — missing days parse as 0:00 instead of the 8:30 default. Add `break`/`else`. |
| M2 | `src/main.cpp` L137–157, L448, L615 | AP setup uses `WiFi.config()` (STA static IP) instead of `WiFi.softAPConfig()`; works only by coincidence of defaults. `WiFi.localIP()` in AP mode is 0.0.0.0 — the setup prints and the `;ip:` device description show the wrong IP (use `WiFi.softAPIP()`). `isAccessPoint = true` is set before `softAP()` can fail. |
| M3 | `src/chip_info.h` L7; `src/sensors/one_wire.h` | ODR sweep (D1) incomplete: `getChipInfo()` is a non-inline definition in a header; `one_wire.h` defines globals/functions with no include guard and no `inline`. Latent link errors on second inclusion. Also `chip_info.h` reports "WiFi/BT" from `CHIP_FEATURE_BT` alone. |
| M4 | `src/config.cpp` L125–145, L163–200 | `readConfigAsString()` and `setConfig()` are dead code (no callers); `readConfig()`/`readConfigAsString()` retry by unbounded recursion after writing a default config — worn flash that acks writes but fails reads recurses to stack overflow. Retry a bounded N times. |
| M5 | `src/webpage.cpp` L98–118 | Embedded HTML closes `</body></html>` mid-page (form/iframe outside, second close later). Config form submits the whole JSON as a GET query string — URL-encoding can exceed the request-line buffer, silently truncating; `submitConfig()` shows success regardless of outcome. |
| M6 | `src/config.cpp` L537–551 | Light color deserialization has no per-field defaults: a config with only `LightLow` turns `LightMedium`/`LightHigh` black (null → 0) instead of using defaults. Use the `\| default` pattern used elsewhere. |
| M7 | `src/main.cpp` L307–319 | `triggerEvents()` (WindSpeed > 4.0 → `CallEvent`) is never called — the wind-speed relay trigger silently does nothing. |
| M8 | `src/main.cpp` L73, L359 | `sensorOffsets` is never populated — the calibration feature is inert; `operator[]` just default-inserts 0.0 per name. |
| M9 | `src/led/ledstrip.cpp` L3, L47; `src/main.cpp` L229 | `LedStrip` constructed with default `NumberOfLEDs = -1` → `uint16_t` 65535 → Adafruit_NeoPixel tries to malloc ~197 KB (silently fails without PSRAM, pins ~192 KB with). Repeats via `updateLength()` on every "off". Guard with `max(0, n)` or skip creation when `<= 0`. |
| M10 | `src/led/ledstrip.cpp` L100–117 | `updateLEDs` change detection compares scaled vs stored *unscaled* values — skip works only at factor 1.0 (where it can wrongly suppress a needed frame); elsewhere `show()` runs every tick (each briefly disables interrupts). |
| M11 | `src/led/ledstrip.cpp` L214–221 | `showError()` never shows red (`m_LedColor` is write-only) and would block 5–20 s. Currently dead code — fix or delete. |
| M12 | `src/handle_buttons.cpp` L88–97; `src/led/button_inputs.cpp` L35; `src/sensors/sensors.cpp` L117 | Presses arriving during a slow handler (blocking `CallEvent` HTTP) are erased (`pressed = false` after handler); noisy releases > 200 ms later count as a second press. `pin > 0` checks make GPIO 0 unusable for buttons and one-wire (sentinel is −1; should be `>= 0`). |
| M13 | `src/led/leds_service.cpp` L83–96 | `/api/led` GET response JSON is hand-built without escaping the echoed `Message` — a `"` or `\` in the request message yields invalid JSON. Brightness can also report 105–120 during a sunrise (factor up to 1.2). Serialize with ArduinoJson. |
| M14 | `src/led/sunrise_alarm.cpp` L44–58 | Alarm triggers on minute equality gated only by `!m_IsAlarmActive`: a sub-minute `SunriseLightTime` restarts the alarm repeatedly within the trigger minute; conversely a loop stall across the whole minute (blocking MQTT/HTTP) skips the alarm entirely. Track "already fired this minute". |
| M15 | `src/handle_buttons.cpp` L14–25 | Silencing the sunrise via button returns before `sendStateTopic()` — Home Assistant keeps showing the lamp ON until something else republishes. |
| M16 | `src/sensors/windsensor.cpp` L32–36 | `getSpeed()` divides by `(millis() - lastTime) / 1000.0` — two calls within the same millisecond yield inf/NaN marked valid. Latent (single caller today). |
| M17 | `src/sensors/sensors.cpp` L207–209 | MH-Z19 version buffer `char version[4]` printed without NUL terminator — OOB read. Use `char version[5] = {0}`. |
| M18 | `src/timehelper.cpp` L72–79 | `getTimestamp()` appends `millis() % 1000` as the `.mmm` fraction — uptime, unrelated to wall-clock sub-second; timestamps can go backwards within a second. Use `gettimeofday()` or drop the fraction. |
| M19 | `src/logging.h` L19; `src/timeseries/timeseries.h` L47–49 | `CLogger::m_IsOnline` not initialized by the constructor (works only because `main.cpp` assigns immediately); `Sensor()` default ctor leaves `Offset` indeterminate. Initialize both. |
| M20 | `src/timeseries/ts_mqtt.cpp` L12; `ts_mqtt.h` L36 | Module-level `MqttClient *mqttClient` is shared by all `CTimeseriesMQTT` instances (second instance repoints the first); `m_Host` is a dead member; the base ctor prepends `http://` to the MQTT server string (harmless only because unused). |

---

## Caveats (behavioral quirks, not bugs)

Things a user of this firmware should know. Documented here; several are candidates for
redesign (see [DESIGN.md](DESIGN.md)).

**Power / deep sleep**
- Deep sleep waits for a buffer flush: the device stays awake `BufferedValues × MeasureInterval`
  (default 3 × 30 s) every wake cycle, because RAM (and the value counter) resets on wake.
  Set `BufferedValues = 1` on battery devices.
- Offline mode + sensors + `DeepSleepTime > 0` kills the AP: the send-gate returns true as
  soon as any value exists, so the deep-sleep branch fires and the access point disappears.

**Network / API**
- `/get` and `PUT /api/config` do **not** restart; WiFi/pin changes take effect only after
  a manual `/restart`. Only `AlarmSettings` are re-applied live.
- Missing string keys deserialize to the literal string `"null"` (ArduinoJson
  `.as<String>()`); only `tryConnect` special-cases it — a `"null"` `SensorID` becomes the
  hostname and timeseries prefix.
- `WiFi.persistent(true)` writes credentials to flash/NVS on connect — they survive a
  config reset done via `config.json` alone.
- `collectBody` allocates `calloc(Content-Length + 1)` from the client-supplied header —
  repeated near-limit bodies fragment the heap on ESP8266.

**MQTT**
- MQTT LED control requires `UseMQTT = true`, which *also* switches the timeseries backend
  to MQTT — you can't have HTTP timeseries + MQTT light control.
- MQTT keepalive is only serviced when `NumberOfLEDs > 0` — a sensor-only MQTT device
  never polls and the connection times out between publishes.
- Broker down + `UseMQTT` on: the loop performs a blocking `connect()` roughly every
  500 ms with no backoff — LED animations and buttons stutter for each TCP timeout.
- `{"state":"ON"}` without a color right after boot turns the lamp on **black** (cached
  colors start at `{0,0,0}` and are never seeded from the strip).
- `mqtt_events::setup()` runs even when `UseMQTT` is false (subscriptions exist but the
  client never connects).

**Sensors**
- SCD30/SCD40 readings at or below 0 °C (or 0 %RH) are dropped as invalid
  (`if (temperature > 0)`) — an outdoor station stops reporting temperature in winter.
  SCD30 CO2 below 400 is clamped to 400.
- If an SCD40 is present, no other I2C sensor is scanned (acknowledged in a comment).
- `getEnv()` calls `bmp.begin()` on every read — resets the sampling/filter config set at
  init and adds ~500 ms per read.
- Counter semantics are read-destructive and inconsistent: `watersensor::getClicks()`
  returns delta-and-reset, `windsensor::getClicks()` cumulative, `getSpeed()` resets the
  averaging window and peak on read. Any second consumer of `sensor::getValues()` corrupts
  rain/wind data; in offline mode the loop consumes and discards rain deltas.
- Pre-NTP samples are stored with 1970-epoch timestamps and shipped to the server
  (`addValue` only rejects *empty* timestamps).
- `TwoWire MyWire = Wire;` copies the I2C-bus-0 object; mixing in a library that uses the
  global `Wire` double-drives the bus.

**LED / buttons**
- During the alarm, the sunrise animates at the loop's 500 ms cadence (the alarm branch
  ignores `runModeAction()`'s requested 100 ms) and logs "Alarm active" every iteration.
- Sensor-driven pulse and API-selected pulse (Mode 5) use different parameter sets — same
  mode name, visibly different behavior.
- Button 1: presses > 5 s apart toggle off; within 5 s they cycle low→medium→high→off.
  Pressing during the alarm's trigger minute before it starts will start-then-cancel the
  sunrise (`handleButton1` probes via side-effecting `run()`).
- Buttons use `INPUT` + FALLING with **no internal pull** — an external resistor is
  required or the pin floats and fires spuriously.
- On ESP32-S3 R8 modules, GPIO 33–37 conflict with octal PSRAM (`qio_opi`) — see the
  warning in `platformio.ini` (boot loop: `assert failed: ets_timer_arm`).

---

## Notes

- Items marked `[ ]` are unresolved. Change to `[x]` (and move to the archive table) when
  fixed and committed.
- Suggested fixing order: C7–C9 first (crashes/hangs), then B7/B9/B10/B15 (silent data
  loss and dead setup flows), then the B4 overflow sweep in one commit, then the rest.
- For C-severity items, write a regression test (or at minimum a serial-log assertion)
  before closing.
- C6 follow-up resolved: `git log -S` over all branches shows the leaked password was
  never committed; no history rewrite or router password rotation forced by the repo.
