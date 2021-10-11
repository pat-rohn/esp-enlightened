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

char const *sensorID = "Wemos2";
char const *timeseriesAddress = "MyIPAddress";
char const *port = "3004";

unsigned long scanrate = 30000;
unsigned long buffersize = 30;

// 1 Wire DHT Pin
//D0   = 16;
//D1   = 5;
//D2   = 4;
//D3   = 0;
//D4   = 2;
//D5   = 14;
//D6   = 12;
//D7   = 13;
//D8   = 15;
uint8_t DHT_PIN = 0;
```

## Example Wemos with DHT22
![alt text](https://raw.githubusercontent.com/pat-rohn/wemos-d1-lite/main/wemosd1dht22.png)
