# ESP32 / ESP8266 Project for using sensors and LED-Strips (WS28xx)
Small platform.io project reading out multiple sensors or controlling LED's using WS28xx.
Sensor data can be sent using http-request. A simple backend written in Go exists:
[go-iotserver](https://github.com/pat-rohn/go-iotedge)


## Get Started
This project is using Platform IO. To build project, first run commands described in lib/README.md to get dependencies

## Configure Device
- Access point will be created and device will have IP 192.168.4.1
- Connect to http://192.168.4.1 and change configuration to your needs (JSON)
- Device will reboot

## Example Wemos with DHT22
![alt text](https://raw.githubusercontent.com/pat-rohn/wemos-d1-lite/main/wemosd1dht22.png)

## Pin Outs
- https://randomnerdtutorials.com/esp8266-pinout-reference-gpios/
- https://randomnerdtutorials.com/esp32-pinout-reference-gpios/
- https://learn.adafruit.com/adafruit-huzzah32-esp32-feather/pinouts
