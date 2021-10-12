#include <Arduino.h>
#include <array>
#include <map>

#ifdef ESP8266
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#endif /* ESP8266 */

#ifdef ESP32
#include "WiFi.h"
#include <HTTPClient.h>
#endif /* ESP32 */


#include "configuration.h" // TODO: Create this file, see README

#include "timeseries.h"

#include "sensors.h"
#include "leds_service.h"


CTimeseries timeseries = CTimeseries(timeseriesAddress, port);
CLEDService ledService = CLEDService(LED_PIN);

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

void startLedControl()
{
  ledService.beginPixels();
  ledService.beginServer();
  ledService.fancy();
}

void setup()
{
  Serial.begin(115200);
  Serial.println("setup");

  pinMode(LED_BUILTIN, OUTPUT);

  if (connectToWiFi())
  {
    if (sensorsInit(DHT_PIN))
    {
      hasSensors = true;
      return;
    }
    hasSensors = false;
    startLedControl();
    return;
  }
  else
  {
    WiFi.disconnect();
    if (!WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS))
    {
      ledService.showError();
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
  startLedControl();
}

unsigned long lastUpdate = millis();
unsigned long counter = 0;

void measureAndSendSensorData()
{
  if (millis() > lastUpdate + scanrate)
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
      if (WiFi.status() != WL_CONNECTED)
      {
        if (!connectToWiFi())
        {
          Serial.println("No WiFi Connection");
          return;
        }
      }
      digitalWrite(LED_BUILTIN, 0x0); // to check
      timeseries.sendData();
      counter = 0;
      digitalWrite(LED_BUILTIN, 0x1);
    }
  }
}

void loop()
{
  if (isAccessPoint || !hasSensors)
  { // is LED control
    ledService.listen();
    return;
  }
  measureAndSendSensorData();
}
