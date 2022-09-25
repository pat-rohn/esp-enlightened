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

const bool kTryFindingSensors = true;
const bool kIsOfflineMode = false;
const String &kSensorID = "Outdoor";
const String &kTimeseriesAddress = "my.server.ch";
const String &kTimeseriesPort = "3004";

const int kNrOfLEDs = -1;


```

## Example Wemos with DHT22
![alt text](https://raw.githubusercontent.com/pat-rohn/wemos-d1-lite/main/wemosd1dht22.png)

## Pin Outs


https://randomnerdtutorials.com/esp8266-pinout-reference-gpios/
https://randomnerdtutorials.com/esp32-pinout-reference-gpios/
