# esp-multi — Design Decisions & Improvement Roadmap

Companion to [BUGS.md](BUGS.md) (concrete defects). This document records the
architectural decisions the codebase embodies, their trade-offs, and a prioritized plan
for improving the project as a whole.

---

## Architecture overview

One firmware image for many device roles (sensor node, LED lamp, sunrise alarm clock,
weather station), selected at **runtime** by a JSON config stored in LittleFS
(`/config.json`).

```
main.cpp (setup/loop, global singletons)
├── configman        — config load/save/serialize (config.cpp, LittleFS JSON)
├── webpage          — ESPAsyncWebServer: setup page, /api/config, /api/led, /restart, /api/version
├── sensor           — auto-detection + polling of DHT22, BME/BMP280, SHT30, SGP30,
│                      MH-Z19, SCD30, SCD40, DS18B20, wind/rain click counters
├── timeseries       — CTimeseries base with two backends:
│   ├── ts_http      — buffered POST to go-iotedge (/timeseries/save, /init-device)
│   └── ts_mqtt      — immediate publish per value
├── led              — LedStrip modes (on/off/campfire/colorful/sunrise/pulse),
│                      CLEDService (API glue), CSunriseAlarm, button ISRs
├── mqtt_events      — Home-Assistant-style light control (json/set, state topics)
└── logging / timehelper — remote log messages, NTP time
```

Communication between the async web server task and the main loop goes through
`std::atomic<bool>` flags (`restartTriggered`, `configChanged`, `buttonPressed1/2`)
consumed by `checkWebpageTriggers()`.

---

## Design decisions as found in the code

Each decision, why it (probably) was made, and what it costs.

### 1. One firmware, runtime feature selection
All sensor drivers and the whole LED stack are always compiled in; the config decides
what runs. **Why:** one image to flash for every device; reconfigure without rebuilding.
**Cost:** every build pulls ~15 libraries; flash/RAM footprint is paid on every device;
compile-time flags exist anyway (`USE_ALL_SENSORS`, `ENABLE_SCD40`), so the model is
already half-broken.

### 2. `platformio.ini` is untracked; `platformio-examples.ini` is the shared template
`.gitignore` excludes `platformio.ini` so each machine keeps a local env (upload port,
board). **Why:** per-device settings don't churn the repo. **Cost:** fresh clones don't
build until the user copies an example env; nothing documents that step (the README
pointed to an empty `lib/README.md`). Note: `platformio-examples.ini` currently contains
`[env:featheresp32]` **twice** — PlatformIO rejects duplicate sections when the file is
used as-is.

### 3. Restart-to-apply configuration, with one live exception
`POST /api/config` restarts; `PUT` and the legacy `/get` form save without restart, and
only `AlarmSettings` are re-applied live via the `configChanged` flag. **Why:** re-running
`setup()` is the simplest way to re-init pins/drivers. **Cost:** WiFi/pin/LED changes
silently do nothing until a manual restart (documented in API_CONFIG.md); and the config
handoff itself races with the reader (BUGS C7).

### 4. Access-point fallback for provisioning
Unconfigured or offline devices open an AP at 192.168.4.1 with a built-in HTML form.
**Why:** field setup without tooling. **Cost:** the AP path is the least-tested one —
see BUGS C8 (offline crash), B9 (dead `/restart` while unconfigured), M2 (wrong IP calls).

### 5. Single cooperative loop, ISRs only set flags
No FreeRTOS tasks are created; everything runs in `loop()` gated by `millis()` deadlines
(default 500 ms tick), buttons interrupt via ISR → flag. **Why:** simplicity; portability
to ESP8266. **Cost:** every blocking call (HTTP POST, MQTT connect, NTP wait) freezes
LED animations, button response, and the sunrise ramp; the alarm can even miss its
trigger minute (BUGS M14). The deadline arithmetic is also the recurring overflow-bug
source (BUGS B4).

### 6. Two timeseries backends behind one base class — but not actually polymorphic
`CTimeseries` holds the data model; HTTP buffers (`BufferedValues`) and flushes,
MQTT publishes immediately and ignores buffering (BUGS D3). `initDevice()` exists only on
the HTTP class, forcing an invalid downcast in `main.cpp` (BUGS B7). **Why:** the MQTT
backend was bolted on later. **Cost:** the abstraction leaks; callers must know the
concrete type, and semantics differ per backend.

### 7. Recreate-the-world reconfiguration with raw `new`/`delete`
`configureDevice()` deletes and re-creates all singletons. **Why:** guarantees clean state
after config changes. **Cost:** every forgotten `delete` is a leak (webPage was one),
every consumer of the old pointer a use-after-free risk; no ownership model.

### 8. Trusted-LAN security model
No auth on any endpoint, CORS `*` everywhere, WiFi password readable via
`GET /api/config` and printed on serial while unconfigured. **Why:** home-network
convenience. **Cost:** any browser tab on the LAN can reconfigure or restart the device
(BUGS D2, D5).

### 9. Sensor auto-detection with "found once, never re-checked" semantics
`sensorsInit()` probes buses and registers whatever answers; `loop()` retries only until
the *first* success. **Why:** zero-config sensor wiring. **Cost:** false positives freeze
the sensor set forever (BUGS B10, B24); detection order encodes hidden priorities (SCD40
suppresses the whole I2C scan).

### 10. Firmware version generated at build time
`extra_scripts/version_gen.py` writes `version_generated.h` (date + git hash), exposed at
`GET /api/version`. Good decision — keep it. The generated file is correctly gitignored.

### 11. Hardware constraint: octal PSRAM vs GPIO 33–37
On ESP32-S3 R8 modules, enabling `qio_opi` PSRAM makes GPIO 33–37 part of the PSRAM bus;
using them (e.g. flamelight buttons on 35/36) corrupts the bus → `ets_timer_arm` assert
boot loop. Decision taken: **PSRAM stays off** in the default env; documented in
`platformio.ini`. Devices needing PSRAM must use a separate env and keep GPIO 33–37 free.

---

## Improvement roadmap

Ordered by value-for-effort. BUGS.md IDs in brackets.

### P0 — Stabilize (bug-fix pass, no redesign)
1. Fix the three criticals: config data race [C7], offline null-deref [C8], SCD30 boot
   hang [C9].
2. One sweep replacing every `millis() + N` deadline with subtraction form [B4, C9].
3. Correct the silent-data-loss cluster: HTTP status check [B15], buffer cap with
   eviction [B16], DHT/DS18B20/SHT30 validity [B10, B13, B23], BME280 unit [B11].
4. Guard the setup/AP path: triggers while unconfigured [B9], `softAPConfig` [M2].

### P1 — Ownership and concurrency model
- **Config:** web handlers only *stage* a validated `Configuration`; the main loop
  performs the swap and the LittleFS write. This fixes C7 structurally, not just with a
  lock, and makes "which changes apply live vs. after restart" an explicit list.
- **Timeseries:** make `CTimeseries` a real interface (`newValue`, `sendData`,
  `initDevice` all virtual); give MQTT buffered/batch semantics or an explicit
  "unbuffered" capability flag [B7, D3].
- **Singletons:** replace the delete/new dance with `std::unique_ptr` members of a single
  `Device` struct; reconfiguration = rebuild that struct.

### P2 — Security (small, high value)
- Pre-shared token (`X-Authorization`) on all mutating endpoints, 401 otherwise [D2].
- Drop CORS `*` on mutating endpoints; keep it for GET if the web UI needs it.
- Stop serializing the WiFi password into `GET /api/config` responses and serial dumps
  [D5]; redact in `serializeConfig` unless explicitly requested.

### P3 — Robustness of the loop
- Introduce a tiny non-blocking scheduler (function + next-due-time table) instead of the
  single `nextInterval`; move HTTP/MQTT sends toward async or bounded-timeout calls so
  the sunrise/LED animations never stall.
- MQTT reconnect with exponential backoff; poll independent of `NumberOfLEDs`.
- Alarm trigger: compare against "last fired" timestamp instead of minute equality [M14].

### P4 — Code quality and hygiene
- Delete dead code: `triggerEvents`, `readConfigAsString`, `setConfig`, `showError`,
  `sensorOffsets` (or implement offsets for real) [M4, M7, M8, M11].
- Finish the header/ODR sweep (`chip_info.h`, `one_wire.h`) [M3].
- Replace hand-built JSON with ArduinoJson everywhere [M13]; audit `String`
  concatenation in hot paths (heap fragmentation on ESP8266).
- Initialize all members in constructors [M19]; enable `-Wall -Wextra` (and fix what it
  reports) in the PlatformIO envs.

### P5 — Testing & CI
- Extract pure logic (config (de)serialization, color/brightness math, timestamp
  formatting, buffer eviction) behind Arduino-free interfaces and cover it with
  `pio test -e native` unit tests — the C1/C2-style self-assignment bugs are exactly what
  these catch.
- GitHub Actions matrix build for the example envs (`platformio-examples.ini`, after
  de-duplicating `featheresp32`) so every PR proves all boards still compile.
- Optionally clang-tidy/cppcheck in CI; the uninitialized-read class of bugs ([B17],
  [B19], [M19]) is machine-findable.

### P6 — Features worth considering
- **OTA updates** (ArduinoOTA or a `/update` endpoint) — currently every fix needs USB
  access, which is painful for installed alarm lamps.
- mDNS (`sensorid.local`) so users don't need to hunt IPs.
- Config schema version field + migration, so old `config.json` files upgrade cleanly.
- Home Assistant MQTT discovery instead of hand-configured topics.

### Repo hygiene (quick wins)
- De-duplicate `[env:featheresp32]` in `platformio-examples.ini`.
- Add `serial-out.log` (and similar capture files) to `.gitignore`.
- Document the "copy an env from `platformio-examples.ini` into `platformio.ini`" setup
  step in the README (done — the empty `lib/README.md` it used to point to was removed).
- Keep `API_CONFIG.md` / `API_LED.md` in sync when endpoints change; they are the de facto
  API contract.
