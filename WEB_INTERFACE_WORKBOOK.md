# Web Interface and Refactoring Workbook

## Purpose

Plan a configuration-first web interface for an Enlightened device and a
low-risk refactoring path for the firmware. This workbook is intentionally
planning-only: it makes no production-code or API changes.

This workbook is a companion to [DESGIN.md](DESGIN.md) (architecture
decisions and improvement roadmap) and [BUGS.md](BUGS.md) (concrete
defects). Where a planned step here depends on fixing a specific bug or
follows a specific roadmap phase from those documents, the ID/phase is
cited in brackets, e.g. `[D2]`, `[DESIGN P2]`.

## Current State

The device serves a small HTML form from `src/webpage.cpp`. The existing
`/api/config` endpoints are the right integration point for a new interface,
but the current implementation has known defects that a new interface must
not inherit:

| Endpoint | Current behavior | Interface use | Known issues |
| --- | --- | --- | --- |
| `GET /api/config` | Returns the complete serialized configuration | Load the editable settings | Includes the plaintext WiFi password with no auth check `[D2]`; any browser tab on the LAN can read it because of wildcard CORS `[D2]` |
| `PUT /api/config` | Validates and stages configuration without restart | Save changes | Same unauthenticated-write exposure `[D2]`; only `AlarmSettings` is re-applied live — WiFi/pin/LED config changes silently do nothing until a manual restart (see Caveats in BUGS.md) |
| `POST /api/config` | Validates, stages, then requests a restart | Save-and-restart, if retained | Same exposure as `PUT` |
| `GET /restart` | Requests a restart and returns a redirect page | Explicit restart action | — |
| `GET /api/version` | Returns firmware version | Device status | — |
| `GET /api/time` | Returns device time | Device status | — |
| `GET/POST/PUT /api/led` | Reads/sets LED mode, color, brightness | Optional live LED preview/control (Decision 4) | Hand-built JSON response doesn't escape the echoed `Message` field — invalid JSON on a `"`/`\` in the request `[M13]`; `Mode: 4` (sunrise) can read an uninitialized start time and jump straight to "risen" `[B19]` |
| `GET /api/button1`, `/api/button2` | Simulates a physical button press | Not planned for v1 | — |
| `GET /get` | Legacy query-string config submit used by the current HTML form | Superseded by `PUT /api/config` | Submits the whole config as a URL — can exceed the request-line buffer and truncate silently `[M5]` |

The current embedded page (`src/webpage.cpp` inline HTML) is **not** a
reliable visual or structural reference: its `</body></html>` closes mid-page
with a stray form/iframe outside it, and the submit handler reports success
regardless of the actual server response `[M5]`. Treat it as legacy to be
replaced, not refactored in place.

`configman::stageConfig()` and `configman::applyStagedConfig()` already keep
async HTTP handlers from directly mutating configuration or writing flash
(this is the fix landed for `[C7]`, the config data race). That handoff must
remain intact and is the model the wider `services/configuration_service`
split (below) should generalize, not replace.

### Security posture that gates this work

Per `DESGIN.md` decision 8 and `BUGS.md` `[D2]`, there is currently **no
authentication on any endpoint** and CORS is `Access-Control-Allow-Origin: *`
on every response, including mutating ones. This is flagged in `DESGIN.md` as
"worse than originally noted": any web page open in a browser on the same LAN
can read the WiFi password or reconfigure/restart the device via CSRF, not
just a deliberate LAN client. Shipping a nicer web interface on top of this
without addressing it just makes the attack surface more attractive and more
discoverable. See Decision 3 and Phase 0 below — this is treated as a
blocking prerequisite, not an optional hardening pass.

## Companion App: `enlightened`

`/home/schusti/workspace/enlightened` is not just a visual reference — it is
a real, existing Angular/Ionic/Capacitor mobile app (Android, per its
`README.md`) that is the *actual current client* of this firmware's HTTP API.
Its own README states it is "Compatible with esp-enlightened", and
`src/app/services/ledcontrol.service.ts` calls exactly the endpoints in the
Current State table above (`GET/PUT /api/config`, `GET/POST /api/led`,
`GET /api/button{1,2}`, `GET /api/time`, `GET /restart`) with plain
`http://<device-address>` requests. This means any change to the API surface
here is a change to a live client, not a hypothetical one. Two concrete
findings from reading its source:

- **It sends no `Authorization`/`X-Authorization` header at all.** The
  planned Phase 0 fix for `[D2]` (require a pre-shared token on mutating
  endpoints) will break this app's config-save, LED-control, and
  restart-device features unless the app is updated in lockstep, or the
  firmware accepts a transitional unauthenticated grace mode. This must be
  sequenced explicitly, not assumed away by "existing API clients ... remain
  compatible" in the acceptance checklist.
- **Its `DeviceSettings` model (`src/app/settings.ts`) is missing fields that
  the firmware serializes and deserializes:** `AnalogSensorPin0`,
  `AnalogSensorPin1`, `OneWirePin`, `DeepSleepTime`, `BufferedValues`, and
  `MeasureInterval` all exist in `config.h`/`config.cpp` but have no
  counterpart in the app's TypeScript interface. Since the app always PUTs
  its full in-memory `DeviceSettings` object back (`saveDeviceSettings()` /
  `applyDeviceSettings()` in `ledcontrol.service.ts`), saving settings from
  the phone app today silently resets those six fields to firmware defaults
  on every save — a real, currently-live version of the "must submit the
  complete configuration shape" risk called out in Validation Rules below,
  not just a risk for the new web interface. Worth a note back to the
  `enlightened` project regardless of this workbook's own scope.

Both points argue for treating the new embedded web interface and the
`enlightened` app as two clients of one contract: any endpoint/schema change
made under this workbook's refactoring sequence should be checked against
`enlightened`'s source, not just against this repo's own tests.

## Design Direction

Use the companion app in `/home/schusti/workspace/enlightened` as the visual
reference, not as a runtime dependency:

- Ionic palette: primary `#3880ff`, secondary `#3dc2ff`, success `#2dd36f`,
  danger `#eb445a`, light neutral surfaces.
- Calm blue header, white cards, clear labels, compact controls, responsive
  single-column layout on phones and two columns on wider screens.
- Plain web controls and no external font, JavaScript, CSS, or CDN dependency;
  setup must work when the device access point has no internet connection.
- The device password is editable but never rendered in static HTML or logged
  by the page script. This also requires a server-side fix: `GET /api/config`
  currently returns the WiFi password in plaintext and the AP-mode loop dumps
  the full serialized config (password included) to Serial every 15 s
  `[D5]`. The UI cannot "not log" a value the firmware already exposes
  unconditionally — redacting/omitting the password in `serializeConfig()`
  unless explicitly requested is a prerequisite, not a UI-only concern.
- If Decision 3 (below) requires authentication, the page needs a lightweight
  token-entry affordance (e.g. a single password-style field gating all
  actions, sent as `X-Authorization`) rather than a full login flow, matching
  the pre-shared-token approach in `DESGIN.md [P2]`.


### Information Architecture

1. **Header and device summary**
   - Product name, firmware version, connection state, device time.
   - Refresh action and clear, non-blocking load/save/error feedback.
   - In AP/setup mode, do not rely on `WiFi.localIP()` for any displayed or
     printed address — it reports `0.0.0.0` in AP mode today because setup
     uses `WiFi.config()` instead of `WiFi.softAPConfig()` `[M2]`. This must
     be fixed server-side before the "device summary" or "device time" IP
     hints can be trusted in setup mode.

2. **Connection**
   - Device name, Wi-Fi SSID/password, server address.
   - Configured, offline-mode, and show-webpage switches.

3. **Hardware and sensors**
   - LED count/data pin; discovery; serial, analog, DHT, OneWire, wind,
     rainfall, and button pins.
   - Hide dependent values until the parent capability is enabled (for
     example, MQTT fields behind `UseMQTT`).

4. **Automation and light**
   - Sunrise enablement, duration, per-day alarm time/enablement.
   - Low, medium, and high RGB values displayed as deliberate color controls,
     not opaque JSON.

5. **Telemetry and MQTT**
   - Measurement interval, buffering, deep sleep.
   - MQTT enablement, topic, and port.

6. **Actions**
   - `Save changes` uses `PUT /api/config`, stays on the page, then reloads the
     accepted configuration.
   - Because only `AlarmSettings` is currently re-applied live (see Current
     State), the save flow must tell the user which changed fields need a
     restart to take effect, rather than implying every save is immediately
     active. A conservative first cut: treat every field outside
     `AlarmSettings` as restart-required and show a persistent "restart
     needed" banner after any such change, until the ownership work in
     Phase 3/4 makes the live-vs-restart set explicit and machine-readable.
   - `Restart device` requires confirmation and uses the existing restart
     endpoint. A separate `Save and restart` action is optional; it should use
     the existing `POST /api/config` behavior only after its resulting restart
     experience is specified.
   - Invalid responses must be shown as errors, never reported as success —
     this is a direct fix for the current form's behavior, which reports
     success unconditionally regardless of the server response `[M5]`.

### Validation Rules to Surface in the UI

The server remains the source of truth. The browser should add early feedback
for required values, numeric fields, port ranges, valid `http`/`https` URLs,
and RGB values from 0 through 255. It must submit the complete configuration
shape because the current deserializer is configuration-document based rather
than patch based. Note the deserializer also has silent per-field default
gaps that the UI can't fully compensate for and should not mask: a payload
that only sets `LightLow` currently zeroes `LightMedium`/`LightHigh` instead
of keeping their defaults `[M6]`, and a day missing from `AlarmSettings`
silently becomes `00:00` instead of the documented `08:30` default `[M1]`.
Submitting the complete, previously-loaded shape (not a hand-built payload)
sidesteps both until they are fixed server-side.

## Web Delivery Decision

| Option | Recommendation | Why |
| --- | --- | --- |
| Embedded HTML/CSS/JS in `PROGMEM` | Start here | No build pipeline, single firmware artifact, compatible with ESP8266/ESP32. |
| Gzipped static assets in LittleFS | Follow-up if the interface grows | Better source separation and browser caching, but adds upload/build coordination. |
| Full Ionic/Angular bundle | Do not use on-device | Asset size, memory, and no-network setup constraints conflict with the embedded web server. |

Even while embedded, source should be authored as separate UI files and bundled
by a PlatformIO pre-build step into a generated header. Generated output should
not be hand-edited. This keeps the firmware C++ readable and allows isolated UI
syntax checks. Because ESP8266 targets are already flash/RAM constrained
(`DESGIN.md` decision 1: ~15 libraries always compiled in), add a binary-size
check to CI/build once the generated header exists, rather than discovering an
overflow only when flashing a device.

## Target Firmware Structure

The objective is clearer ownership, not a wholesale framework rewrite. This
structure is this workbook's concrete proposal for `DESGIN.md`'s **P1 —
Ownership and concurrency model** roadmap item, and directly replaces decision
7's "recreate-the-world" pattern (`configureDevice()` deleting and
re-creating all singletons with raw `new`/`delete`, the source of the `webPage`
leak fixed under `[B3]` and a standing use-after-free risk for every other
consumer of the old pointers).

```text
src/
  app/
    device_controller.{h,cpp}       # Owns startup, loop orchestration, restart
    runtime_state.{h,cpp}           # Explicit flags/events shared with adapters
  domain/
    configuration.{h,cpp}           # Types, defaults, validation, serialization
    lighting.{h,cpp}                # RGB and sunrise value types
    sensor_readings.{h,cpp}         # Sensor values and identifiers
  services/
    configuration_service.{h,cpp}   # Stage/apply/persist lifecycle
    telemetry_service.{h,cpp}       # Buffer and dispatch sensor readings
    lighting_service.{h,cpp}        # LED and sunrise application
  adapters/
    web/
      web_server.{h,cpp}            # Routes and request/response helpers only
      web_assets.generated.h        # Generated, embedded interface artifact
    storage/
      littlefs_config_store.{h,cpp} # LittleFS I/O only
    network/
      wifi_manager.{h,cpp}
      mqtt_client.{h,cpp}
      http_timeseries_client.{h,cpp}
  drivers/
    sensors/
    leds/
    buttons/
  main.cpp                          # Creates DeviceController and delegates
```

### Ownership Boundaries

| Concern | Target owner | Must not own |
| --- | --- | --- |
| HTTP routes, CORS, request decoding | `adapters/web` | Global device pointers, persistence, business decisions |
| Configuration validation and migration | `domain/configuration` | LittleFS, HTTP request types |
| Staging/applying/saving config | `services/configuration_service` | HTTP response rendering |
| Wi-Fi/AP and MQTT connections | `adapters/network` | Configuration parsing |
| Sensor/LED GPIO interaction | `drivers` | Web or persistence behavior |
| Startup sequence and loop | `app/device_controller` | Protocol-specific request handling |

`CWebPage` currently relies on static global pointers for LED service, time,
and flags. The first refactoring seam should replace these setters with a
constructor that receives narrow interfaces or a `DeviceController` command
port. The web server should ask for commands such as `stageConfiguration`,
`requestRestart`, and `pressButton`; it should not access GPIO, LED state, or
global configuration directly.

## Refactoring Sequence

Each step is independently buildable and should preserve endpoints and
configuration serialization unless a migration is explicitly introduced. This
sequence follows `DESGIN.md`'s roadmap ordering: bug stabilization (P0) before
ownership work (P1), security (P2) as a hard gate before exposing a nicer UI,
and only then the new interface itself.

0. **Fix the bugs this interface would otherwise inherit or make worse**
   *(maps to `DESGIN.md [P0]`/`[P2]`; do this before or alongside step 1, not
   after)*
   - Security, because a friendlier UI increases exposure: add the
     pre-shared-token check (`X-Authorization`, HTTP 401 otherwise) on
     mutating endpoints and drop wildcard CORS on them `[D2]`; redact the
     WiFi password from `serializeConfig()` output and stop the periodic
     Serial dump of the full config while in AP mode `[D5]`.
   - Setup-mode correctness the UI depends on: fix AP addressing to use
     `WiFi.softAPConfig()`/`WiFi.softAPIP()` instead of the station-mode
     `WiFi.config()`/`WiFi.localIP()` `[M2]`.
   - Config deserialization gaps a "load then re-save whole document" UI
     would otherwise silently propagate: per-field light-color defaults
     `[M6]` and the missing-day alarm default `[M1]`.
   - If LED controls are in scope (Decision 4): fix the unescaped `Message`
     echo in the `/api/led` JSON response `[M13]` and the uninitialized
     sunrise start time on `Mode: 4` `[B19]`.
   - Not required to unblock the UI, but cheap to fold into the same pass
     since it touches the same file: the legacy `/get` endpoint's
     unauthenticated GET-based config submit `[M5]` should be retired once
     the new interface's `PUT` flow replaces it, rather than kept as a
     second unauthenticated write path.

1. **Establish a regression baseline**
   - Capture representative configuration JSON documents, including legacy
     documents with absent optional fields.
   - Add host-testable tests for deserialize/serialize round trips, rejected
     invalid JSON, defaults, and staged configuration replacement.
   - Document the API response status and body for every endpoint, including
     `/api/led`, `/api/button1`, `/api/button2`, and legacy `/get`, which the
     original endpoint table in this workbook omitted.

2. **Untangle configuration**
   - Move configuration types, defaulting, validation, and serialization into
     `domain/configuration`.
   - Isolate LittleFS reads/writes behind a configuration store.
   - Keep the existing thread-safe staging handoff (the `[C7]` fix); expose it
     through a configuration service instead of the `configman` global
     namespace.

3. **Introduce application composition**
   - Create `DeviceController` to own setup order and loop tasks now located
     in `main.cpp`, replacing decision 7's raw `new`/`delete`
     `configureDevice()` reconstruction.
   - Move restart and button atomic flags into named runtime command/state
     types.
   - Replace raw owning pointers with `std::unique_ptr` where platform
     constraints permit, making lifetime explicit.

4. **Split transport adapters**
   - Extract web routes from `webpage.cpp` into focused route registration
     functions: configuration, LED, status, and command routes.
   - Deduplicate `POST`/`PUT` body parsing and CORS responses; this is also
     where the step-0 auth/CORS fix and the `/get` retirement land
     structurally rather than as a patch on the existing monolith.
   - Keep all configuration persistence on the loop/application side.

5. **Separate sensor, lighting, and telemetry workflows**
   - Extract measurement scheduling, LED color policy, and event triggers from
     `main.cpp` into services.
   - Give `CTimeseries` a real virtual interface (`newValue`/`sendData`/
     `initDevice`) so MQTT and HTTP backends stop diverging in buffering
     semantics `[D3]`; the previous HTTP-only downcast for `initDevice()` was
     already patched with a `!UseMQTT` guard `[B7]`, but the underlying
     abstraction leak remains and is a better fix.
   - Keep hardware-specific calls in drivers.

6. **Implement the planned web interface**
   - First ship read/load behavior against the unchanged API.
   - Add editable groups and `PUT` save behavior, including the
     restart-required-fields banner described in Information Architecture
     item 6.
   - Add restart and optional save-and-restart after manual AP and station-mode
     testing.
   - Only then consider LittleFS-hosted assets if binary size or source
     maintainability warrants it.

## Acceptance Checklist for the Future Implementation

- [ ] Works at `192.168.4.1` while offline and with JavaScript enabled, using
      the corrected AP IP addressing `[M2]`.
- [ ] Shows clear loading, saved, validation-error, and connection-error
      states.
- [ ] Renders all serialized configuration fields, including nested sunrise and
      RGB settings.
- [ ] Preserves all fields when editing one field and saving.
- [ ] Does not log or otherwise expose the Wi-Fi password beyond the active
      edit control, and the server no longer returns or logs it unredacted
      either `[D5]`.
- [ ] `PUT /api/config` applies the configuration without an unexpected
      restart; restart is explicit; fields that are not live-applied are
      clearly marked as restart-required in the UI.
- [ ] Mutating endpoints require the pre-shared token and reject
      cross-origin writes; only safe `GET`s keep permissive CORS `[D2]`.
- [ ] Existing API clients, including the companion application, remain
      compatible — verified against `enlightened`'s actual
      `ledcontrol.service.ts`, not assumed; in particular its lack of any
      `Authorization` header must be resolved (grace mode, coordinated app
      update, or documented breaking change) before the `[D2]` auth fix
      ships, and its `DeviceSettings` model should be updated to include
      `AnalogSensorPin0/1`, `OneWirePin`, `DeepSleepTime`, `BufferedValues`,
      and `MeasureInterval` so saving from the app stops silently resetting
      them.
- [ ] Builds for the supported ESP32 and ESP8266 environments within existing
      flash/RAM budgets.

## Decisions Needed Before Implementation

1. Should `Save and restart` be a primary workflow or should configuration
   always save without a restart? (Constrained by the fact that today only
   `AlarmSettings` applies live — see Current State.)
2. Which pin fields should be hidden, disabled, or marked advanced for each
   target board?
3. Is an unauthenticated configuration page acceptable on a station network,
   or should setup mode be the only configuration mode? **Given `[D2]`'s
   severity (any LAN-adjacent browser tab can read the WiFi password or
   trigger a restart today), the recommendation is to require the
   pre-shared-token auth fix in Phase 0 regardless of which answer is
   chosen** — it is a prerequisite for exposing a nicer, more discoverable
   UI either way, not an optional extra tied to one answer. This decision
   also determines whether the `enlightened` companion app needs a
   coordinated update before the token check is enforced, since it currently
   sends no auth header (see Companion App section above).
4. Should the web page include LED controls and current sensor readings, or
   stay strictly configuration-focused in the first release? If yes, `[M13]`
   and `[B19]` must be fixed first (see Phase 0).
