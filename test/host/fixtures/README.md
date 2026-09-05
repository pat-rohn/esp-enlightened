# Configuration contract fixtures

These fixtures capture JSON documents accepted or produced by the current
configuration API. The host test suite validates their JSON shape and the
redacted-secret contract without compiling Arduino `String` or `LittleFS`.

- `current-redacted-config.json` represents `GET /api/config`: it has the
  complete document shape, but `WiFiPassword` and `ApiToken` are blank and
  their presence is reported by `HasWiFiPassword` and `HasApiToken`.
- `legacy-minimal-config.json` is the minimum configuration accepted by the
  firmware. Missing optional values retain the firmware deserializer defaults.
- `partial-light-config.json` covers the M6 regression: a document that
  provides only one `LightLow` channel must leave the other light channels at
  their firmware defaults.

The later configuration-domain extraction should replace the schema-only
checks with direct deserialize/serialize round-trip and default-value tests
using these same fixtures.
