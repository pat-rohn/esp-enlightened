# ESP32 / ESP8266 Project for using sensors and LED-Strips (WS28xx)
Small platform.io project reading out multiple sensors or controlling LED's using WS28xx.
Sensor data can be sent using http-request. A simple backend written in Go exists:
[go-iotserver](https://github.com/pat-rohn/go-iotedge)


## Get Started
This project is using Platform IO. To build project, first run commands described in lib/README.md to get dependencies

## Configure Device
- Access point will be created and device will have IP 192.168.4.1
- Connect to http://192.168.4.1 and change configuration to your needs (JSON)
- To stay connected (on Android) configure static IP (e.g. 192.168.4.5/16 - 255.255.0.0) and use DNS1 0.0.0.0
- Device will reboot

## Example Wemos with DHT22
![alt text](https://raw.githubusercontent.com/pat-rohn/wemos-d1-lite/main/wemosd1dht22.png)

- 
