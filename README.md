# wemos-d1-lite
Small platform.io project reading out multiple sensors or controlling LED's using WS28xx.
Sensor data can be sent using http-request. A simple backend written in Go exists:
[go-iotserver](https://github.com/pat-rohn/go-iotedge)

## Get started:
- Run commands described in lib/README.md to get dependencies.
- Create a file named "configuration.h" with following content

```
std::map<std::string, std::string> wifiData{
    {"MyWiFi", "X"},
};
char const *sensorID = "Wemos2";
char  *timeserver = "pool.ntp.org";
char const *timeseriesAddress = "<MybackendIpAddress.ch>";
unsigned long scanrate = 30000;
unsigned long buffersize = 50;
```

## Example Wemos with DHT22
![alt text](https://raw.githubusercontent.com/pat-rohn/wemos-d1-lite/main/wemosd1dht22.png)
