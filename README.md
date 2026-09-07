# ESP32 / ESP8266 Project for using sensors and LED-Strips (WS28xx)
Small PlatformIO project reading out multiple sensors or controlling LEDs using WS28xx.
Sensor data can be sent via HTTP request or MQTT. A simple backend written in Go exists:
[go-iotserver](https://github.com/pat-rohn/go-iotedge)

## Get Started
This project uses [PlatformIO](https://platformio.org/). `platformio.ini` is intentionally
untracked (it holds machine-specific ports); copy a matching `[env:...]` section from
[platformio-examples.ini](platformio-examples.ini) into your own `platformio.ini`, then
build — PlatformIO downloads all `lib_deps` automatically.

## Configure Device
- Access point will be created and device will have IP 192.168.4.1
- Connect to http://192.168.4.1 and change configuration to your needs (JSON)
- To stay connected (on Android) configure static IP (e.g. 192.168.4.5/16 - 255.255.0.0) and use DNS1 0.0.0.0
- Device will reboot


## Over-the-Air (OTA) Updates
The firmware runs an [ArduinoOTA](https://docs.platformio.org/en/latest/platforms/espressif32.html#over-the-air-ota-update)
listener, so a device that is already on your WiFi can be reflashed without a USB cable.

### Enabling it
OTA is started when **both** of these hold:

- the device is in station mode (not the fallback access point),
- `IsOfflineMode` is `false`.

It is **not** gated on `ApiToken`. If a token is set, OTA reuses it as the upload password — the
same token that guards the configuration endpoints — and the serial console reports `OTA enabled
(password protected)`. If no token is set, OTA still listens, with no password, and says so
loudly on the console.

> **Security note:** with no `ApiToken` configured, any host that can reach the device on the
> network can flash arbitrary firmware onto it. Set an `ApiToken` to require a password.

OTA used to require a non-empty `ApiToken`, which made a stray token unrecoverable: the token
gates the configuration endpoints, so clearing it took OTA down with it and left USB as the only way
back in. Clearing a token is now an explicit `"ClearApiToken": true` in `PUT /api/config` (a blank
`ApiToken` still means "keep the stored one", because `GET` redacts it).

OTA is only brought up during `setup()`, so **restart** the device after changing `ApiToken`. The
OTA hostname is the device's `SensorID`, advertised over mDNS, so give every device a unique
`SensorID`.

### Uploading
Point PlatformIO at the device instead of a serial port:

```ini
[env:my_device_ota]
; ... same platform/board/build_flags as your USB env ...
upload_protocol = espota
upload_port = 192.168.1.50        ; or <SensorID>.local
upload_flags =
	--auth=<your ApiToken>
```

```console
pio run -e my_device_ota -t upload
```

The default OTA ports are 3232 on ESP32 and 8266 on ESP8266; `espota` picks the right one per
platform. A wrong token fails with `Authentication Failed`.

### Caveats
- **ESP8266 (1 MB modules such as `d1_mini_lite`) cannot use OTA.** ArduinoOTA stages the new
  image in free flash before `eboot` copies it over, so the image must fit *next to* the running
  one. The current firmware is ~511 KB of a 958 KB sketch region, leaving ~447 KB free — less
  than it needs, so `Update.begin()` fails with `Not Enough Space`. Flash these boards over USB,
  or move to a 2 MB+ module.
- **ESP32 flash headroom is tight.** With the default two-slot partition table the app partition
  is 1.25 MB and the current build already uses ~86 % of it. Switching to a single-slot table
  (`huge_app.csv`) to gain room removes the second slot and disables OTA altogether. The 16 MB
  layout in `src/default_16MB.csv` has plenty of space.
- **No OTA in access-point mode.** A device that fell back to its own access point (e.g. wrong
  WiFi credentials) can only be recovered over USB.
- **`ApiToken` is a single shared secret.** Anyone who can reach the device with that token can
  flash arbitrary firmware, and the configuration page keeps it in the browser's `localStorage`.
  The image itself is neither signed nor encrypted and travels the LAN in plaintext — treat OTA
  as trusted-network only.
- **Deep sleep shrinks the window.** With `DeepSleepTime > 0` the device is only reachable during
  the few seconds it is awake; keep retrying, or disable deep sleep while updating.
- Uploading interrupts sensor readings and LED output for the duration of the transfer, and the
  device reboots afterwards.


## Known Hardware Caveat
On ESP32-S3 R8 modules, do **not** enable octal PSRAM (`qio_opi`) while using GPIO 33–37
(e.g. buttons on 35/36) — those pins belong to the PSRAM bus and the device ends in a
boot loop (`assert failed: ets_timer_arm`). Details in the comment block in
`platformio.ini` / [platformio-examples.ini](platformio-examples.ini).


## Example Wemos with DHT22
![alt text](https://raw.githubusercontent.com/pat-rohn/wemos-d1-lite/main/wemosd1dht22.png)

- 
