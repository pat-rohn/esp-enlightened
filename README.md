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

const String &kSensorID = "Sensor1";
const String &kTimeseriesAddress = "my-server.ch";
const String &kTimeseriesPort = "3004";


bool kIsOfflineMode = false;
bool kTryFindingSensors = true;
int kNrOfLEDs = 1;


// 1 Wire DHT Pin WEMOS D1 mini
//RX   = 3
//TX   = 1
//D0   = 16;
//D1   = 5; // SCL
//D2   = 4; // SDA
//D3   = 0; (DHT) 
//D4   = 2; (LED)
//D5   = 14;
//D6   = 12;
//D7   = 13;
//D8   = 15;
// For ESP32
//adafruit-huzzah32-esp32 https://learn.adafruit.com/adafruit-huzzah32-esp32-feather/pinouts
// https://gist.github.com/iamamused/968694bea91e087b7d85fe9f1a7cca99


```

## Example Wemos with DHT22
![alt text](https://raw.githubusercontent.com/pat-rohn/wemos-d1-lite/main/wemosd1dht22.png)
