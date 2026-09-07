# Firmware HTTP contract check

Run against a real device after it is reachable:

```sh
python3 test/integration/api_contract.py --base-url http://192.168.4.1
```

For a token-enabled device, provide the locally configured token:

```sh
python3 test/integration/api_contract.py \
  --base-url http://192.168.4.1 \
  --token "$ENLIGHTENED_API_TOKEN"
```

The default suite is read-only. It checks:

| Endpoint | Expected result |
| --- | --- |
| `GET /api/config` | 200 JSON, complete public config shape, redacted secrets, boolean `Has*` flags |
| `GET /api/led` | 200 JSON with color, brightness, mode, and message |
| `GET /api/version` | 200 non-empty text |
| `GET /api/time` | 200 `H:M (weekday N)` text |
| `GET /get` | 404 (the legacy endpoint is retired) |
| `PUT /api/config` with an over-declared `Content-Length` | device stays up (no reboot) |

The last row is a regression probe: it declares `Content-Length: 10` and then
sends 4000 bytes. `collectBody` sizes its buffer from `Content-Length`, and
ESPAsyncWebServer 3.6.x (the floor of our `^3.6.0` range) forwards whatever the
peer sent, so before the bounds check this wrote ~1300 bytes into an 11 byte
allocation and panicked the device with `LoadProhibited` on corrupted heap
metadata — reachable without a token, because the body is collected before any
handler runs `isAuthorized`. Library 3.11+ clamps chunks itself, so this probe
only bites on the older resolution; the bounds arithmetic is covered on the
host by `test/host/test_request_body.cpp`.

`--mutations` additionally exercises safe validation paths: an invalid
configuration POST and LED POST must return 400, while PUT re-submits the
loaded configuration and must return a redacted 200 response. Use it only
when no other client is concurrently editing the device.

`--commands` also presses button 1, presses button 2, then restarts the
device; it is intentionally destructive and therefore requires
`--mutations --commands`. It must be run last in a manual pre-release check.

Use `--expect-auth` on a token-enabled device to prove an unauthenticated
mutating request is rejected with 401 before authenticated checks are run.
