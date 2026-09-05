# Host tests

Run the host-only suite from the repository root:

```sh
pio test -c platformio-tests.ini -e native
```

`platformio.ini` is deliberately ignored because it contains local board and
serial-port configuration. `platformio-tests.ini` is tracked so contributors
and CI can run the same host test environment.

The first test verifies the ArduinoJson dependency in native mode. Firmware
configuration currently depends directly on Arduino `String` and `LittleFS`,
so compiling `src/config.cpp` on the host would require an inaccurate
filesystem/String shim. The configuration extraction phase must move
defaulting, validation, and serialization into a platform-independent module;
its unit tests belong here once that boundary exists.
