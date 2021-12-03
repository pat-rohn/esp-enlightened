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

CTimeseries timeseries = CTimeseries(kTimeseriesAddress, kTimeseriesPort);
LedStrip ledStrip = LedStrip(kLEDPin, kNrOfLEDs);
CLEDService ledService = CLEDService(&ledStrip);

bool hasSensors = false;

IPAddress local_IP(192, 168, 4, 1);
IPAddress subnet(255, 255, 0, 0);
IPAddress primaryDNS(8, 8, 8, 8);
IPAddress secondaryDNS(8, 8, 4, 4);
IPAddress gateway(192, 168, 4, 1);

bool isAccessPoint = false;

const char *ssidAP = "AI-Caramba";
const char *passwordAP = "ki-caramba";

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
  if (kNrOfLEDs > 0)
  {
    Serial.println("startLedControl");
    ledStrip.beginPixels();
    ledStrip.apply();
    ledStrip.fancy();
    if (hasSensors)
    {
      Serial.println("Pulse Mode");
      ledStrip.m_LEDMode = LedStrip::LEDModes::pulse;
    }
  }
}

void setup()
{
  Serial.begin(115200);
  Serial.println("setup");

  pinMode(LED_BUILTIN, OUTPUT);
  if (sensorsInit(kDHTPin, kRX ,kTX))
  {
    hasSensors = true;
  }
  if (!kIsOfflineMode)
  {
    if (!connectToWiFi())
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
        setup();
      }
    }
    ledService.beginServer();
  }
  startLedControl();
}

void setCO2Color(double co2Val)
{
  // good: 0-1500 (white to yellow)
  // medium: 1500-4000 (yellow to red)
  // bad:4000:8000 (red to dark)
  double blue = (1.0 - (co2Val - 500) / 1000) * 100.0;
  if (blue > 100.0)
  {
    blue = 100;
  }
  if (blue < 0)
  {
    blue = 0;
  }
  double green = (1.0 - (co2Val - 1500) / 2500) * 100.0;
  if (green > 100.0)
  {
    green = 100;
  }
  if (green < 0)
  {
    green = 0;
  }
  double red = (1.0 - (co2Val - 4000) / 4000) * 100.0;
  if (red > 100.0)
  {
    red = 100;
  }
  if (red < 0)
  {
    red = 0;
  }

  ledStrip.setColor(red, green, blue);
}

void setTemperatureColor(double temperature)
{
  double blue = (1.0 - (temperature - 15) / 5) * 100.0;
  if (blue > 100.0)
  {
    blue = 100;
  }
  if (blue < 0)
  {
    blue = 0;
  }
  double green = (1.0 - (temperature - 20) / 5) * 100.0;
  if (green > 100.0)
  {
    green = 100;
  }
  if (green < 0)
  {
    green = 0;
  }
  double red = (1.0 - (temperature - 25) / 5) * 100.0;
  if (red > 100.0)
  {
    red = 100;
  }
  if (red < 0)
  {
    red = 0;
  }
  ledStrip.setColor(red, green, blue);
}

unsigned long lastColorChange = 0;
const double kColorUpdateInterval = 120000;
double co2TestVal = 400;
double tempTestVal = 15;

void colorUpdate()
{
  if (ledStrip.m_LEDMode != LedStrip::LEDModes::pulse && millis() > lastColorChange + kColorUpdateInterval)
  {
    lastColorChange = millis();
    //co2TestVal *= 1.1;
    //setCO2Color(co2TestVal);
    //tempTestVal += 1.0;
    //setTemperatureColor(tempTestVal);
    //return;

    auto values = getValues();
    if (values.empty())
    {
      Serial.println("No Sensors");
      return;
    }
    if (values.count("CO2"))
    {
      setCO2Color(values["CO2"].value);
    }
    else if (values.count("Temperature"))
    {
      setTemperatureColor(values["Temperature"].value);
    }
  }
}

unsigned long lastUpdate = millis();
unsigned long counter = 0;

void measureAndSendSensorData()
{
  if (millis() > lastUpdate + kSensorScanRate)
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
        String name = kSensorID;
        timeseries.addValue(name + it->second.name, it->second.value);
      }
    }
    counter++;
    if (counter >= kSensorBuffersize)
    {
      if (WiFi.status() != WL_CONNECTED)
      {
        if (!connectToWiFi())
        {
          Serial.println("No WiFi Connection");
          return;
        }
      }
      timeseries.sendData();
      counter = 0;
    }
  }
}

void loop()
{
  if (hasSensors && !isAccessPoint && !kIsOfflineMode)
  { // should have connection to timeseries server
    measureAndSendSensorData();
  }
  if (kNrOfLEDs <= 0)
  {
    return;
  }
  colorUpdate();
  if (kIsOfflineMode)
  {
    ledStrip.runModeAction();
  }
  else
  {
    ledService.listen();
  }
}
