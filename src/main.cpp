#include <Arduino.h>
#include <array>
#include <map>

#include <ESP8266WiFi.h>
#include <Arduino.h>

#include "configuration.h"

#include "timeseries.h"

#include "sensors.h"
#include "leds_service.h"

/*

std::map<std::string, std::string> wifiData{
    {"MyWiFi", "X"},
};

char const *sensorID = "Wemos2";
char const *timeseriesAddress = "MyIPAddress";
char const *port = "3004";

unsigned long scanrate = 30000;
unsigned long buffersize = 50;
*/

CTimeseries timeseries = CTimeseries(timeseriesAddress, port);

bool hasSensors = false;

IPAddress local_IP(192, 168, 4, 1);
IPAddress subnet(255, 255, 0, 0);
IPAddress primaryDNS(8, 8, 8, 8);
IPAddress secondaryDNS(8, 8, 4, 4);
IPAddress gateway(192, 168, 4, 1);

bool isAccessPoint = false;

const char *ssidAP = "AI-Caramba";
const char *passwordAP = "ki-caramba";

uint32_t SENSOR_SCANRATE = 20 * 1000L;
unsigned long target_time = 0L;

SensorType sensorType = SensorType::unknown;

bool tryConnect(std::string ssid, std::string password)
{
  Serial.print("Try connecting to: ");
  Serial.print(ssid.c_str());
  Serial.println("");
  WiFi.begin(ssid.c_str(), password.c_str());

  int counter = 0;
  while (WiFi.status() != WL_CONNECTED && counter <= 15)
  {
    digitalWrite(LED_BUILTIN, 0x0);
    delay(250);
    Serial.print(".");
    digitalWrite(LED_BUILTIN, 0x1);
    delay(250);
    counter++;
    if (counter >= 15)
    {
      return false;
    }
  }
  Serial.println("");
  Serial.print("Connected to WiFi network with IP Address: ");
  Serial.println(WiFi.localIP());
  WiFi.setAutoReconnect(true);
  WiFi.persistent(true);
  return true;
}

bool connectToWiFi()
{
  WiFi.disconnect();
  for (auto const &x : wifiData)
  {
    if (tryConnect(x.first, x.second))
    {
      return true;
    }
  }
  return false;
}

void setup()
{
  Serial.begin(115200);
  delay(500);
  Serial.println("begin");

  pinMode(LED_BUILTIN, OUTPUT);

  if (connectToWiFi())
  {
    if (sensorsInit())
    {
      hasSensors = true;
      return;
    }

    beginPixels();
    beginServer();
    digitalWrite(LED_BUILTIN, 0x1);
    fancy();
    return;
  }
  else
  {
    WiFi.disconnect();
    if (!WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS))
    {
      showError();
      Serial.println("STA Failed to configure");
      setup();
    }
    Serial.print("Has access point:");
    Serial.println(WiFi.localIP());
    isAccessPoint = true;
    if (!WiFi.mode(WIFI_AP))
    {
      Serial.print("mode failed.");
    }
    if (!WiFi.softAP(ssidAP, passwordAP))
    {
      Serial.print("softAP failed.");
    }
  }
  beginPixels();
  beginServer();
  digitalWrite(LED_BUILTIN, 0x1);
  fancy();
}

unsigned long lastUpdate = millis();
unsigned long counter = 0;

void loop()
{
  unsigned long currentTime = millis();
  if (hasSensors && currentTime > lastUpdate + scanrate)
  {
    lastUpdate = millis();

    auto values = getValues();
    std::vector<String> valueNames;
    std::vector<float> tsValues;
    std::map<String, SensorData>::iterator it;
    if (values.empty())
    {
      Serial.println("No Values");
    }
    for (it = values.begin(); it != values.end(); it++)
    {
      if (it->second.isValid)
      {
        String name = sensorID;
        timeseries.addValue(name + it->second.name, it->second.value);
      }
    }
    counter++;
    if (counter > buffersize)
    {
      timeseries.sendData();
      counter = 0;
    }
  }
  if (isAccessPoint || !hasSensors)
  {
    listen();
    return;
  }

  if (millis() - target_time <= SENSOR_SCANRATE)
  {
    return;
  }

  target_time += SENSOR_SCANRATE;
  digitalWrite(LED_BUILTIN, 0x1);
  if (isAccessPoint)
  {
    return;
  }
  delay(150);
  if (WiFi.status() != WL_CONNECTED)
  {
    if (!connectToWiFi())
    {
      Serial.println("No WiFi Connection");
      return;
    }
  }

  digitalWrite(LED_BUILTIN, 0x0);
}
