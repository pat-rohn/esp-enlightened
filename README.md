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


## Known Hardware Caveat
On ESP32-S3 R8 modules, do **not** enable octal PSRAM (`qio_opi`) while using GPIO 33–37
(e.g. buttons on 35/36) — those pins belong to the PSRAM bus and the device ends in a
boot loop (`assert failed: ets_timer_arm`). Details in the comment block in
`platformio.ini` / [platformio-examples.ini](platformio-examples.ini).


## Example Wemos with DHT22
![alt text](https://raw.githubusercontent.com/pat-rohn/wemos-d1-lite/main/wemosd1dht22.png)

- 
