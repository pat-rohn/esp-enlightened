# Web Control Interface Workbook

## Purpose

Make the device's own page **control-first**: light and alarm on the start
page, configuration one tab away — matching what the companion app already
offers.

Getting there is mostly an API problem, not a UI problem. The current API grew
one endpoint at a time around a whole-document configuration write and a
half-applied token, and both the device page and the app pay for that in
client-side complexity. Backwards compatibility is **dropped**, so this
workbook replaces the API with a smaller one and updates both clients in
lockstep.

Paired documents:

- [../enlightened/API_SIMPLIFICATION_WORKBOOK.md](../enlightened/API_SIMPLIFICATION_WORKBOOK.md)
  — the app-side half of the same change. **Read together; they share one
  contract.**
- [WEB_INTERFACE_WORKBOOK.md](WEB_INTERFACE_WORKBOOK.md) — the configuration
  interface that exists today, and the refactoring backlog.
- [DESGIN.md](DESGIN.md), [BUGS.md](BUGS.md) — cited as `[D*]`, `[M*]`, `[F*]`.

## Decisions That Change

Superseding earlier decisions, on the user's instruction:

| Was | Now |
| --- | --- |
| `WEB_INTERFACE_WORKBOOK.md` Decision 4: configuration-only embedded UI, no live LED control | Control-first. Light and alarm are the start page |
| Token gates writes only; `/api/led`, `/api/button*`, all `GET`s ungated | **If a token is set, it is required for every `/api/*` request and `/restart`.** One rule, no exceptions |
| Stored config document and API frozen for compatibility | Both may change. The app is updated in the same release |
| Writes send the whole configuration document | Writes are **partial**; absent keys keep their stored value |
| `UX_WORKBOOK.md`'s "the app must keep working against firmware that has none of it" | Dropped. Mixed versions are not supported and fail loudly |
| ESP8266 is a first-class target whose flash budget shapes the interface | **ESP32 only.** ESP8266 code stays, but stops constraining the design |
| A version mismatch surfaces as a bare `401`/`404` | `/api/status` reports `ApiVersion`; the app checks it once and says so plainly |
| Light and alarm only | Plus a **compact sensor strip**, which needs a new `Sensors` block in `/api/status` |

## Target API

Seven endpoints, one auth rule, one write pattern.

### Auth

`GET /` is **ungated** — a browser cannot attach a header to a navigation, and
the page is what lets you type the token in the first place. It is a static
asset and discloses nothing.

Everything else: when `Configuration.ApiToken` is non-empty, the request must
carry `X-Authorization: <token>` or it is `401`. Reads included. An empty token
means the device is unprotected, which stays the default so first-time setup
over the access point works.

### Endpoints

| Method | Path | Body | Returns |
| --- | --- | --- | --- |
| `GET` | `/` | — | The page (ungated, gzipped) |
| `GET` | `/api/status` | — | Everything live: version, uptime, heap, WiFi, time, **light**, alarm |
| `GET` | `/api/config` | — | Stored configuration, secrets omitted |
| `PUT` | `/api/config` | Partial config | Stored configuration + `RestartRequired` |
| `POST` | `/api/led` | Partial light | The `Light` block |
| `POST` | `/api/alarm/test` | `?seconds=` | `{Message, DurationSeconds}` |
| `POST` | `/api/button/1`, `/api/button/2` | — | `{Message}` |
| `POST` | `/restart` | — | `{Message}` |

**Removed:** `POST /api/config` (save-and-restart — restart is explicit),
`GET`/`PUT /api/led` (status covers the read; one write verb is enough),
`GET /api/time` and `GET /api/version` (both folded into `/api/status`),
`GET /api/button{1,2}` (a `GET` must not mutate), `GET /restart`, the
form-urlencoded body fallback in `getInput()`, and the `%`-template processor.

The only non-test consumers of the removed routes are
[api_contract.py](test/integration/api_contract.py) and the app; nothing in the
OTA path touches them.

### `GET /api/status` — the single read

```json
{
  "ApiVersion": 2,
  "Version": "2026-09-09-abc1234",
  "UpTimeSeconds": 1234, "FreeHeap": 41000, "SensorID": "PaddyLight",
  "WiFi":  { "Connected": true, "SSID": "…", "RSSI": -54, "IP": "192.168.1.31" },
  "Time":  { "IsSynced": true, "Local": "06:05", "Weekday": 2, "Utc": 1757… },
  "Light": { "HasStrip": true, "Red": 128, "Green": 70, "Blue": 15,
             "Brightness": 100, "Mode": 1, "Owner": "manual" },
  "Alarm": { "IsActivated": true, "IsRunning": false },
  "Sensors": { "AgeSeconds": 12,
               "Values": [ { "Name": "co2", "Value": 812, "Unit": "ppm" } ] }
}
```

`Light` is new and is what lets both clients open a screen with one request
instead of two. `Owner` is one of `manual`, `sunrise`, `mqtt`, `button` — see
Phase D2.

`ApiVersion` is an integer, bumped on every breaking change. A client reads it
once per device and refuses politely on a mismatch: "this device needs a
firmware update" beats a bare 404 the user has to diagnose. This release is
version `2`.

`Sensors` is a **cached** snapshot, never a live read — see the sensor note
below. `Values` is a list, not fixed keys, because which sensors exist is
discovered at runtime; `AgeSeconds` lets a client grey out a stale strip.
Absent or empty when the device has no sensors, which is the honest answer for
a light-only device.

`Time.Weekday` is Monday=0…Sunday=6
([timehelper.cpp:108](src/timehelper.cpp#L108)), which already matches
`configuration::weekday_t` ([configuration.h:26-35](src/domain/configuration.h#L26-L35)),
so it indexes the schedule directly. The next alarm stays a client-side
computation — the device deliberately does not do date arithmetic
([webpage.cpp:344-352](src/webpage.cpp#L344-L352)), and that reasoning still
holds.

### Partial writes — the change that removes the most code

`PUT /api/config` merges: **a key that is absent keeps its stored value.** So
arming the alarm is:

```json
{ "SunriseSettings": { "IsActivated": true } }
```

…and the response is the full stored configuration, so the client re-renders
from what the device actually kept. `POST /api/led` works the same way — send
`{"Brightness": 40}` and the colour and mode are untouched.

This one rule deletes, in both clients: whole-document read-modify-write,
"blank means keep" secret handling, the save-then-poll-until-it-matches loop,
and the risk of a client silently resetting fields it does not model. On the
firmware side it also closes `[M6]` and `[M1]` — the per-field default gaps —
by construction, because nothing falls back to a default any more.

Secrets: `WiFiPassword` and `ApiToken` are **omitted** from `GET /api/config`,
which keeps `HasWiFiPassword`/`HasApiToken` meaningful (`[F6]`). Sending a key
sets it; omitting it keeps it; sending `""` clears it. Unambiguous, which the
current blank-means-keep rule is not.

`RestartRequired` in the write response tells the client whether the change is
already live. The firmware already knows this — only `AlarmSettings` is
re-applied ([main.cpp:549-556](src/main.cpp#L549-L556)) — it just never said so
(`[F4]`).

### Implementation note

`deserializeConfig()` currently falls back to **defaults** for absent fields,
e.g. `parsed.ShowWebpage = doc["ShowWebpage"] | true`
([configuration_codec.cpp:340](src/domain/configuration_codec.cpp#L340)). Merge
semantics means changing every one of those to fall back to `existing`. It is a
mechanical pass over one file, it is exactly what makes the clients simple, and
the host tests in [test/host](test/host) are there to catch a missed field. The
existing thread-safe `stageConfig()`/`applyStagedConfig()` handoff `[C7]` is
untouched.

## Navigation

One asset, hash routes (`#/light`, `#/alarm`, `#/settings`), tab bar, **Light
default**.

Not a link to a second page: two pages need a second PROGMEM asset and a
duplicated CSS/JS shell inside one flash budget, and the duplication costs more
than the tab bar. Hash routes are deep-linkable (`http://<device>/#/alarm`),
keep the back button working, and need no firmware routing at all.

Order: Light, Alarm, Settings. Light is the most frequent action; Settings is
the existing configuration form, moved as-is.

## Two Hazards Worth Knowing Before Starting

**1. The `%` template processor will corrupt this page.** `/` is served with a
processor callback ([webpage.cpp:404](src/webpage.cpp#L404)), so
ESPAsyncWebServer replaces every `%…%` pair. Today's page contains *exactly
one* `%` (`width:100%`), which is the only reason nothing has broken. A light
UI is full of them — brightness values, `100%` widths, `width:${n}%`. Both
placeholders the processor still answers are unused. Delete it. Doing so also
unblocks gzip, which the processor forbids.

**2. Three writers drive the LED strip with no arbitration**: `/api/led`
([leds_service.cpp:12](src/led/leds_service.cpp#L12)), the sunrise loop
([sunrise_alarm.cpp:32-58](src/led/sunrise_alarm.cpp#L32-L58)), and
`handleMQTT()` ([main.cpp:525-543](src/main.cpp#L525-L543)). Today you discover
this by moving a slider and watching it get overwritten. `Light.Owner` makes it
something the UI can state.

**3. Sensors must not be read from the HTTP task.** `sensor::getValues()`
([sensors.h:46](src/sensors/sensors.h#L46)) does real I2C/OneWire/serial I/O
and is called today only from the main loop, on `MeasureInterval`
([main.cpp:332-340](src/main.cpp#L332-L340)). Calling it from an async request
handler would do blocking bus I/O on the TCP task, racing the loop for the same
buses. So: the loop caches its last `std::map<String, SensorData>` plus a
timestamp, and `/api/status` serves that cache. The strip shows values up to
`MeasureInterval` old, which is what the readings are anyway.

## Frontend Stack

**ESP32-only changes this question.** The 16 KiB cap in
[web_assets.py](extra_scripts/web_assets.py) exists to protect ESP8266 flash
headroom; on a 4 MB ESP32 a gzipped page costs a few KB and the budget stops
being a design constraint. So the framework choice is now about authoring
comfort and build cost, not bytes.

**Decided: vanilla DOM plus a ~40-line hash router.** Three tabs of state card, sliders and seven rows is
what plain DOM does well, and the existing config page already renders a whole
dynamic form in ~90 lines. Svelte is the credible alternative now that budget
is gone: it compiles away, so the runtime cost is ~2 KB. What it costs is a
`web/` npm project, `npx vite build` from the PlatformIO pre-script, and Node
in [firmware.yml](.github/workflows/firmware.yml) — a permanent increase in the
cost of building this firmware, paid for a three-tab page. Preact/lit/Alpine
are the worst of both: a runtime cost *and* no compile-time help.

The build still needs one change: the generator embeds source verbatim today.
Phase D1 adds gzip — and **only** gzip. Minification was in an earlier draft of
this plan; with ESP32-only it buys maybe 10–15% on top of gzip, which does not
justify making the firmware build depend on `rjsmin`/`rcssmin`/`htmlmin` (none
of which are installed here). Python's `gzip` is stdlib, so the generator stays
dependency-free.

## Test Device

`192.168.1.31` answers, and is the reference device for every phase. Probed
read-only on 2026-09-09:

| Fact | Value | Consequence |
| --- | --- | --- |
| Firmware | `2026-09-07-bb5fc99` | **Five commits behind `main`** — `/api/status` (`8da1d4f`) and mDNS (`38b974c`) are not on it. Flash before anything else |
| `ApiToken` | not set (`HasApiToken: false`) | Uniform auth is untested here. Setting one locks the device out of the current app until its release lands — do it deliberately, not incidentally |
| `NumberOfLEDs` / `LEDPin` | 39 / 19 | Real strip; light and sunrise work is verifiable |
| `UseMQTT` / `IsOfflineMode` | true / false | MQTT genuinely writes the strip, so `Light.Owner: mqtt` is a real case, not a theoretical one |
| Sensor pins | **all `-1`** | DHT, OneWire, serial, analog, wind and rain are all disabled. Any I2C sensor would have to come from `FindSensors`. The sensor strip may render empty on this device — see open question 2 |
| Config document | 34 keys | The whole-document write the clients do today |

Because the device is reachable from the dev machine and `platformio.ini`
already has an `espota` env pointing at it, each phase can end **verified on
hardware**, not merely built. Its configuration has been backed up by the
owner, so it may be reconfigured freely — including setting a token to test
the 401 matrix, and enabling sensors.

## Status

All five device phases are implemented and verified on `192.168.1.31`, and the
app's three are implemented in the paired repository. What follows is the plan
as written; the phases are marked with what actually landed.

| Phase | State |
| --- | --- |
| D1 serve the shell safely | Done. 33,695 raw → 10,716 gzipped |
| D2 API v2 | Done. `api_contract.py --mutations` passes against the device |
| D3 shell, tabs, Light | Done |
| D4 Alarm | Done |
| D5 hardening | Done, except **CORS**: the wildcard is kept deliberately, with the reasoning in `addCorsHeaders()` — the Capacitor app is cross-origin, and uniform auth is what actually protects the device |

Two things changed while implementing:

- **Minification was dropped.** ESP32-only plus gzip made it worth ~10-15%,
  which does not justify three build dependencies. The generator is stdlib-only.
- **`Light.Owner` gained a fifth value, `sensor`.** Enumerating the writers
  turned up a real bug: `applySensorColor()` switched the strip to pulse mode
  *before* checking whether any sensor existed, so a device with every sensor
  pin at `-1` had its mode overwritten every measurement interval.

## Phases

### Phase D1 — Serve the shell safely *(firmware + build)*

No visible change; this removes what would break a larger page.

1. Serve `/` without the template processor; delete `CWebPage::processor`.
2. Gzip the asset into a PROGMEM `uint8_t[]` plus a length in
   [web_assets.py](extra_scripts/web_assets.py), using stdlib `gzip` only.
3. Serve it with `Content-Encoding: gzip` via
   `beginResponse(code, type, bytes, len)` — the non-deprecated byte-array
   overload; `beginResponse_P` is marked deprecated in the installed
   ESPAsyncWebServer.
4. Raise the cap to something an ESP32 will not notice and budget the
   **gzipped** size. Print `raw → gzipped / budget`. The old 16 KiB number
   existed for ESP8266 headroom and no longer applies.

**Done when:** today's config page behaves identically and is served gzipped; a
page full of unpaired `%` renders verbatim; verified in a browser against
`192.168.1.31`.

### Phase D2 — API v2 *(firmware, breaking)*

Land the whole contract above in one release; a half-migrated API is worse than
either end of it.

1. Move the `isAuthorized()` check to a single place that runs for every
   `/api/*` route and `/restart`. Delete the per-route decisions and the
   comment block justifying the old asymmetry.
2. Merge semantics in `deserializeConfig()`: absent ⇒ keep `existing`.
3. Omit secrets from `GET /api/config`; keep `HasWiFiPassword`/`HasApiToken`.
4. Return the stored configuration plus `RestartRequired` from
   `PUT /api/config`; return the `Light` block from `POST /api/led`.
5. Add `Light` to `/api/status`, with a `LightOwner` value set at each of the
   four sites that write the strip (`/api/led`, sunrise, MQTT, buttons).
6. Add `ApiVersion: 2` to `/api/status`.
7. Cache the last sensor map in the loop next to `measureAndSendSensorData()`
   and serve it as `Sensors` with an age. No sensor I/O from a request handler.
   Add a build-flag dummy sensor (`-D ENABLE_DUMMY_SENSOR=1`) at the end of
   `sensor::getValues()` ([sensors.cpp:234](src/sensors/sensors.cpp#L234)),
   emitting a slow CO2 ramp over roughly 450–1600 ppm. It costs a dozen lines
   and exercises three things at once on a device with no sensors: the
   `Sensors` block, the UI strip, and `setCO2Color()` in
   [sensor_color_policy.cpp](src/led/sensor_color_policy.cpp), which is the
   CO2 branch of the light indication.
8. Delete the removed endpoints, the form-urlencoded fallback, and their
   `OPTIONS` handlers.
9. Update [api_contract.py](test/integration/api_contract.py) and the host
   tests to the new contract.

**Done when:** contract tests pass against `192.168.1.31`; every removed route
answers 404; with a token temporarily set on the device, every `/api/*` request
without the header answers 401 and `GET /` still serves the page; the token is
then removed again until the app release is ready.

### Phase D3 — Shell, tabs, and the Light tab

1. Split `web/index.html` into a shell (header, tab bar, status line) and three
   views behind a hash router; move the configuration form into `#/settings`
   unchanged.
2. Shared helpers: `api()` (fetch + `X-Authorization` + `Message` extraction),
   the token store (`localStorage`, keyed by origin), `setStatus()`.
3. **One token gate for the page.** Because auth is now uniform, a `401` on any
   call means the whole page is locked: show a single token prompt, store it,
   re-run the failed call. No per-control unlock.
4. Header from `/api/status`: name, connection dot, IP, version, and a warning
   when `Time.IsSynced` is false — an unsynced clock is the likeliest reason an
   alarm never fired `[F9]`. Poll ~10 s, paused while `document.hidden`.
5. Light tab: state card (swatch, On/Off, mode, brightness), power button,
   brightness and R/G/B sliders, mode select
   ([ledstrip.h:38-46](src/led/ledstrip.h#L38-L46)). Empty state when
   `Light.HasStrip` is false. When `Owner` is `sunrise`, say the sunrise is
   driving the light, with "Open alarm" and "Turn alarm off".
6. Presets Low/Medium/High: **Show** previews via `POST /api/led`, **Store**
   writes `{"LightLow":{…}}` — one partial write, no document juggling. Plus
   "press button 1/2" as a wiring test.
7. Compact sensor strip in the header: one line of `Name value unit` from
   `status.Sensors`, greyed when `AgeSeconds` exceeds twice `MeasureInterval`,
   and **absent entirely** when the list is empty — a light-only device must
   not show an empty row.
8. Slider discipline: one in-flight write, coalesce to the latest value,
   throttle ~200 ms while dragging, always commit on release.

**Done when:** every control round-trips and re-renders from the response, not
from the optimistic local value; dragging holds one in-flight request; the page
is usable with a token set and with none.

### Phase D4 — Alarm tab

1. Summary card: armed toggle, next alarm (weekday, time, relative), "starts
   dark at X, fully bright by Y" from `SunriseLightTime`, the `IsRunning` note,
   the clock warning, and an explicit warning for any day whose `AlarmTime` the
   page could not parse — never silently rewrite it.
2. Seven day rows: toggle plus `<input type="time">`, which gives the phone its
   native picker for no bytes.
3. Presets: Mon–Fri, Sat–Sun, Every day, "same time every day".
4. Sunrise length 1–120 minutes, clamped client-side — the firmware accepts a
   zero-length sunrise, which never lights up.
5. Save writes only `{"SunriseSettings": {…}}` and re-renders from the
   response. No read-back loop: the response *is* the stored state.
6. "Test alarm now" → `POST /api/alarm/test`.

**Done when:** a saved schedule survives a restart; a refused time shows the
firmware's own `Message` naming the weekday `[F1]`; the next-alarm line is
correct across a Sunday boundary.

### Phase D5 — Hardening and measurement

1. Narrow CORS from `*` `[D2]`. With uniform auth this is now the last item
   between the browser UI and "security-complete", and it is cheap: the page is
   same-origin, so mutating routes need no cross-origin allowance at all.
2. Test in AP mode at `192.168.4.1` with no internet: every tab, no external
   asset, clock warning correctly present.
3. Build the ESP32 targets and record flash/RAM. ESP8266 examples are no longer
   a gate; if `platformio-examples.ini` still builds, good, but it does not
   block.
4. Record the gzipped asset size for reference.
5. Update `README.md`, `DESGIN.md`'s endpoint list, and mark
   `WEB_INTERFACE_WORKBOOK.md` Decision 4 superseded.

## Landing Sequence

This is a breaking change across two repositories.

0. Flash `192.168.1.31` to current `main` first — it is five commits behind and
   predates `/api/status` entirely.
1. Firmware D1 + D2 merge next, with contract tests green.
2. App phases A1–A3 (see the paired workbook) merge next.
3. Flash the device, then update the app. In between, the app fails **loudly** —
   `401` or `404`, both of which it already classifies and toasts — rather than
   silently writing nothing. That is the whole reason for doing the API break
   as one release instead of several.
4. Device page D3–D4 can land any time after D2; it ships with the firmware.

## Acceptance Checklist

- [ ] Start page is the Light tab; Alarm and Settings one tap away; deep links
      work.
- [ ] With a token set, every `/api/*` request and `/restart` requires it; `GET /`
      does not.
- [ ] Every write is partial and returns the stored state; no client re-sends a
      whole document, and no client polls to confirm a save.
- [ ] No save is reported successful without the device's own response saying
      so, and refusal `Message`s are shown verbatim.
- [ ] `RestartRequired` drives the restart prompt; it is never guessed.
- [ ] An armed alarm and a running sunrise are both visible on the Light tab,
      from `Light.Owner`.
- [ ] Unsynced clock is surfaced wherever it makes an alarm unreliable `[F9]`.
- [ ] Works at `192.168.4.1` offline with no external assets.
- [ ] `ApiVersion` is reported and a mismatched client says so in words.
- [ ] The sensor strip shows real readings, is greyed when stale, and is absent
      on a device with no sensors.
- [ ] No sensor I/O happens on the HTTP task.
- [ ] ESP32 targets build; gzipped asset size recorded.
- [ ] Minification is done by a real minifier, and a template literal in the
      page survives it intact.
- [ ] CORS narrowed `[D2]`.
- [ ] The companion app, at its matching release, passes its own contract
      tests against this firmware.

## Resolved

- **Frontend stack: vanilla.** ESP32-only removed the byte argument, and the
  remaining trade — no toolchain change versus never hand-writing reactive glue
  — was settled in favour of not putting Node in the firmware build.
- **Testing sensors on a device that has none.** A build-flag dummy CO2 sensor
  (Phase D2) rather than wiring hardware; the analog pin stays available as a
  real-signal cross-check if the dummy proves too synthetic.
- **Temperature light indication** may be dropped if it complicates the sensor
  work. It is currently the `else` branch of
  [sensor_color_policy.cpp](src/led/sensor_color_policy.cpp) and costs nothing
  to keep, so the plan keeps it unless it gets in the way.

## Open Questions

1. **Should `POST /api/alarm/test` stay reachable on an unprotected device?**
   It is the one action a passer-by could use to wake someone. Uniform auth
   covers it whenever a token is set; on a tokenless device — which is what
   `192.168.1.31` is today — nothing does.
2. **Config field metadata `[F5]`** — one source shared by the device page and
   the app, instead of two hardcoded groupings that drift. Attractive, and
   bigger than this workbook. Deferred, not rejected.
