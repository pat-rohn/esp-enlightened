# wemos-d1-lite
Small platform.io project reading out multiple sensors or controlling LED's using WS28xx.
Sensor data can be sent using http-request. A simple backend written in Go exists:
[go-iotserver](https://github.com/pat-rohn/go-iotedge)

## Get started:
- Run commands described in lib/README.md to get dependencies.
- Create a file named "configuration.h" with following content

```
#include <string>

std::map<std::string, std::string> wifiData{
    {"MyWiFi", "X"},
};
#define MY_ESP8266
//#define MY_ESP32

bool kIsOfflineMode = false;

const String &kSensorID = "Sensor1";
const String &kTimeseriesAddress = "my-server.ch";
const String &kTimeseriesPort = "3004";

unsigned long kSensorScanRate = 30000;
unsigned long kSensorBuffersize = 50;

int kNrOfLEDs = 1;

// For MY_ESP8266
// 1 Wire DHT Pin WEMOS D1 mini
//D0   = 16;
//D1   = 5;
//D2   = 4;
//D3   = 0;
//D4   = 2;
//D5   = 14;
//D6   = 12;
//D7   = 13;
//D8   = 15;
// For MY_ESP32
//adafruit-huzzah32-esp32 https://learn.adafruit.com/adafruit-huzzah32-esp32-feather/pinouts

const uint8_t kLEDPin = 26;
const uint8_t kDHTPin = 25;
```

## Example Wemos with DHT22
![alt text](https://raw.githubusercontent.com/pat-rohn/wemos-d1-lite/main/wemosd1dht22.png)
