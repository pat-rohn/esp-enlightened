# Web Interface and Refactoring Workbook

## Purpose

Plan a configuration-first web interface for an Enlightened device and a
low-risk refactoring path for the firmware. This workbook began as
planning-only; its Phase 0 stabilization work is now implemented while the
remaining phases continue to guide implementation.

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
| `GET /api/config` | Returns serialized configuration with blank `WiFiPassword`/`ApiToken` plus `HasWiFiPassword`/`HasApiToken` | Load the editable settings | Redacted secret fields require a full-document client to preserve blank values on PUT; CORS remains permissive `[D2]` |
| `PUT /api/config` | Validates and stages configuration without restart | Primary save action | Requires `X-Authorization` once `ApiToken` is configured; only `AlarmSettings` is re-applied live — WiFi/pin/LED config changes need an explicit restart. Refusals answer 400 with the reason in `Message` |
| `POST /api/config` | Validates, stages, then requests a restart | Legacy save-and-restart compatibility endpoint | Requires `X-Authorization` once `ApiToken` is configured; not the new UI's primary path. Refusals answer 400 with the reason in `Message` |
| `GET /restart` | Requests a restart and returns a redirect page | Explicit restart action | Requires `X-Authorization` once `ApiToken` is configured |
| `GET /api/version` | Returns firmware version | Device status | — |
| `GET /api/time` | Returns device time | Device status | — |
| `GET/POST/PUT /api/led` | Reads/sets LED mode, color, brightness | Deferred from configuration v1 (Decision 4) | JSON escaping and sunrise start initialization fixed in Phase 0; mutation requires `X-Authorization` once configured |
| `GET /api/button1`, `/api/button2` | Simulates a physical button press | Not planned for v1 | — |
| `GET /get` | Retired | None | Removed in Phase 0 `[M5]` |

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

Phase 0 added an opt-in `ApiToken` check to mutating routes and redacts
configuration secrets by default. A configured token must be sent as
`X-Authorization`; an empty token preserves existing-device behavior during
migration. CORS is still `Access-Control-Allow-Origin: *` on every response,
including mutating ones. That must be narrowed before a browser-hosted
configuration UI is treated as security-complete; it is not a reason to delay
the configuration-first interface now that the companion app can send a
configured token.

## Companion App: `enlightened`

`/home/schusti/workspace/enlightened` is not just a visual reference — it is
a real, existing Angular/Ionic/Capacitor mobile app (Android, per its
`README.md`) that is the *actual current client* of this firmware's HTTP API.
Its own README states it is "Compatible with esp-enlightened", and
`src/app/services/ledcontrol.service.ts` calls exactly the endpoints in the
Current State table above (`GET/PUT /api/config`, `GET/POST /api/led`,
`GET /api/button{1,2}`, `GET /api/time`, `GET /restart`) with plain
`http://<device-address>` requests. This means any change to the API surface
here is a change to a live client, not a hypothetical one. Phase 0 updated
that client in lockstep:

- **Token support is now coordinated.** `Device` has an optional locally
  stored `ApiToken`, and `LedcontrolService` sends it as `X-Authorization`
  when set. The token is kept locally because firmware GET responses redact
  it; empty-token devices remain compatible during migration.
- **The full configuration model is now preserved.**
  `DeviceSettings` includes `AnalogSensorPin0`, `AnalogSensorPin1`,
  `OneWirePin`, `DeepSleepTime`, `BufferedValues`, and `MeasureInterval`, so
  saving from the phone app no longer resets those firmware fields to
  defaults. It also models the redacted secret indicators and exposes the
  newly added fields in its settings view.

### Alarm times are `HH:MM`, and bad ones are refused `[F1]`

Serialized day settings are zero-padded (`06:05`, never `6:5`), and parsing
accepts `H:M` and `HH:MM` while refusing out-of-range values, non-digits, a
missing or repeated colon, over-long fields and trailing junk. Unpadded input
is still accepted on read, because configurations written by earlier builds
carry it.

Strictness depends on the caller, and any new interface must pick the right
one:

- **HTTP writes parse strictly.** A bad alarm time refuses the whole document
  and `/api/config` answers 400 with the reason in `Message`, naming the
  weekday and quoting the value. An interface must surface that message rather
  than reporting success — the embedded page's current submit handler does the
  opposite `[M5]`.
- **The stored configuration parses leniently.** The offending day keeps its
  previously stored time and the document still loads, because `config.cpp`
  answers a failed load by overwriting the file with defaults, which would
  discard the Wi-Fi credentials along with the bad field.

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
  by the page script. Firmware now redacts `WiFiPassword`/`ApiToken` from
  `GET /api/config` and removes the former AP-mode full-config Serial dump
  `[D5]`; the interface preserves a blank secret field on save.
- The page needs a lightweight token-entry affordance (a single password-style
  field, retained only locally and sent as `X-Authorization`) rather than a
  full login flow, matching the pre-shared-token approach in `DESGIN.md [P2]`.


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
| Embedded HTML/CSS/JS in `PROGMEM` | Start here | Separate `web/` sources are embedded by the pre-build generator; the 16 KiB asset budget preserves ESP8266 flash headroom. |
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
   after)* — **done**, see `BUGS.md`'s archive table for resolution notes.
   - Security, because a friendlier UI increases exposure: add the
     pre-shared-token check (`X-Authorization`, HTTP 401 otherwise) on
     mutating endpoints and drop wildcard CORS on them `[D2]`; redact the
     WiFi password from `serializeConfig()` output and stop the periodic
     Serial dump of the full config while in AP mode `[D5]`.
     — Auth check (`isAuthorized()`) added to `/api/led`, `/api/button1`,
     `/api/button2`, `/api/config` (POST/PUT), and `/restart`; a new
     `Configuration.ApiToken` field gates it, defaulting to empty (i.e. no
     enforcement) so existing installations remain compatible during
     migration; the companion `enlightened` app now sends
     `X-Authorization` when its locally stored token is set. `serializeConfig()`
     now takes a `revealSecrets` flag (default `false`) that redacts
     `WiFiPassword`/`ApiToken` on GET/HTTP responses while flash
     persistence still writes real values; `deserializeConfig()` treats a
     blank incoming secret as "keep the previous value" so a redacted
     GET-then-PUT round trip doesn't wipe the password. **CORS wildcard on
     mutating endpoints was intentionally left as-is** — see `BUGS.md` D2 —
     and remains a required security follow-up before the browser UI is
     considered hardened.
   - Setup-mode correctness the UI depends on: fix AP addressing to use
     `WiFi.softAPConfig()`/`WiFi.softAPIP()` instead of the station-mode
     `WiFi.config()`/`WiFi.localIP()` `[M2]`. — done; added a `currentIP()`
     helper used everywhere the code previously called `WiFi.localIP()`
     while potentially in AP mode.
   - Config deserialization gaps a "load then re-save whole document" UI
     would otherwise silently propagate: per-field light-color defaults
     `[M6]` and the missing-day alarm default `[M1]`. — done.
   - If LED controls are in scope (Decision 4): fix the unescaped `Message`
     echo in the `/api/led` JSON response `[M13]` and the uninitialized
     sunrise start time on `Mode: 4` `[B19]`. — done for both; note B19's
     fix closes the uninitialized-read UB but not the separate issue that an
     API-triggered sunrise is inert while `AlarmSettings.IsActivated` is
     true (documented as a caveat in `BUGS.md`, not in scope here).
   - Not required to unblock the UI, but cheap to fold into the same pass
     since it touches the same file: the legacy `/get` endpoint's
     unauthenticated GET-based config submit `[M5]` should be retired once
     the new interface's `PUT` flow replaces it, rather than kept as a
     second unauthenticated write path. — done; `/get` route removed and the
     broken inline HTML form/iframe replaced with a read-only config view.

1. **Establish a regression baseline**
   - Capture representative configuration JSON documents, including legacy
     documents with absent optional fields.
   - Add host-testable tests for deserialize/serialize round trips, rejected
     invalid JSON, defaults, and staged configuration replacement.
   - Document the API response status and body for every remaining endpoint,
     including `/api/led`, `/api/button1`, and `/api/button2`; verify that
     retired `/get` returns 404.

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
- [ ] Mutating endpoints require the configured pre-shared token. Restrict
      cross-origin writes before declaring the browser UI security-complete;
      only safe `GET`s may keep permissive CORS `[D2]`.
- [ ] Existing API clients, including the companion application, remain
      compatible — verified against `enlightened`'s actual
      `ledcontrol.service.ts`, not assumed. The Phase 0 companion-app update
      supports `X-Authorization` and preserves `AnalogSensorPin0/1`,
      `OneWirePin`, `DeepSleepTime`, `BufferedValues`, and `MeasureInterval`;
      contract coverage must prevent that pairing from regressing.
- [ ] Builds for the supported ESP32 and ESP8266 environments within existing
      flash/RAM budgets.

## Decisions Resolved for Implementation

1. **Save is primary; restart is explicit.** The interface uses
   `PUT /api/config` and clearly marks all hardware, WiFi, and LED topology
   settings as restart-required. It provides a separate restart action after
   a successful save. `POST /api/config` remains only for API compatibility,
   not as the interface's default action.
2. **Pins are advanced, not silently hidden.** The main form shows identity,
   network, telemetry, behavior, and alarm controls. GPIO/pin and LED
   topology settings belong in a collapsed Advanced Hardware section,
   preserving every serialized field while warning that values are
   board-specific and require a restart. The UI must not claim a pin is safe
   across boards; `platformio.ini` board notes remain authoritative.
3. **Token-enabled configuration is the station-network policy.** Setup mode
   remains usable without a token for first-time configuration. Once an
   operator sets `ApiToken`, every mutating request must carry
   `X-Authorization`, and the UI/app retains the token locally because GET
   responses redact it. The companion `enlightened` app is updated in
   lockstep. Restricting wildcard CORS on mutating routes remains an
   outstanding security follow-up before treating the browser UI as hardened.
4. **Configuration-focused v1.** Do not add live LED controls, button
   simulation, or sensor dashboards to the first embedded interface. Preserve
   the API for the companion app and return to live status/control only after
   the configuration flow and CORS policy are tested.

## Phase 7 Status: UI Asset Pipeline

`web/index.html` is the tracked source for the embedded page. The
`pre:extra_scripts/web_assets.py` PlatformIO hook generates an ignored
`src/web_assets.generated.h` containing a `PROGMEM` raw string, which
`webpage.cpp` serves with the existing placeholder processor. The generator
rejects UTF-8 assets larger than 16 KiB so an interface expansion cannot
silently consume the ESP8266 flash headroom.

The `d1_mini_lite` example builds successfully with the ESP8266-compatible
`me-no-dev` async web-server dependencies and the portable configuration
staging handoff. Its current image uses 41,640 of 81,920 bytes RAM (50.8%) and
472,797 of 958,448 bytes flash (49.3%), leaving enough headroom for the
16 KiB asset budget. Adding both targets to the automated build matrix remains
tracked as `fw-test-ci-build-matrix`.

## Phase 8 Status: Configuration Interface

The embedded interface loads `/api/config`, renders every configuration field
in Connection, Hardware and sensors, Telemetry, and Automation and light
groups, and saves the complete document through `PUT /api/config`. It removes
the read-only `HasWiFiPassword`/`HasApiToken` hints before saving and leaves
blank redacted secret fields untouched, preserving the firmware's established
secret-preservation behavior. An optional authorization token is retained only
in browser local storage and sent as `X-Authorization`; restart is a distinct,
confirmed action. The first cut intentionally excludes live LED/button/sensor
controls per Decision 4.
