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

sensor::SensorType sensorType = sensor::SensorType::unknown;

CTimeHelper::Device deviceConfig = CTimeHelper::Device("", 60.0, 3);
std::map<String, float> sensorOffsets;

unsigned long lastUpdate = millis();
int valueCounter = 0;

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

void createAccesPoint()
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
  if (kTryFindingSensors && sensor::sensorsInit(kDHTPin))
  {
    hasSensors = true;
  }

  if (!kIsOfflineMode)
  {
    while (!connectToWiFi())
    {
      Serial.println("Failed to connect to WiFi. Retry..");
    }
    ledService.beginServer();
  }
  else
  {
    createAccesPoint();
  }
  startLedControl();
  deviceConfig = timeseries.init(kSensorID, sensor::getValueNames());
  for (auto const &d : deviceConfig.Sensors)
  {
    sensorOffsets[d.Name] = d.Offset;
  }
  lastUpdate = millis() - deviceConfig.Interval;
}

void setCO2Color(double co2Val)
{
  // good: 0-800 (white to yellow)
  // medium: 800-1000 (yellow to red)
  // bad:1000:1800 (red to dark)
  double blue = (1.0 - (co2Val - 400) / 400) * 100.0;
  if (blue > 100.0)
  {
    blue = 100;
  }
  if (blue < 0)
  {
    blue = 0;
  }
  double green = (1.0 - (co2Val - 800) / 600) * 100.0;
  if (green > 100.0)
  {
    green = 100;
  }
  if (green < 0)
  {
    green = 0;
  }
  double red = (1.0 - (co2Val - 1400) / 1000) * 100.0;
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
  if (ledStrip.m_LEDMode == LedStrip::LEDModes::pulse && millis() > lastColorChange + kColorUpdateInterval)
  {
    lastColorChange = millis();
    //Serial.print("co2TestVal: ");
    //Serial.println(co2TestVal);
    //setCO2Color(co2TestVal);
    //co2TestVal += 100;
    //tempTestVal += 1.0;
    //setTemperatureColor(tempTestVal);
    //return;

    auto values = sensor::getValues();
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



void measureAndSendSensorData()
{
  if (millis() > lastUpdate + deviceConfig.Interval * 1000)
  {
    lastUpdate = millis();
    auto values = sensor::getValues();
    std::vector<String> valueNames;
    std::vector<float> tsValues;
    std::map<String, sensor::SensorData>::iterator it;
    if (values.empty())
    {
      Serial.println("No Values");
    }
    for (it = values.begin(); it != values.end(); it++)
    {
      if (it->second.isValid)
      {
        String name = kSensorID;
        String valueName = name + it->second.name;

        timeseries.addValue(valueName, it->second.value + sensorOffsets[valueName]);
      }
    }
    valueCounter++;
    if (valueCounter >= deviceConfig.Buffer)
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
      valueCounter = 0;
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
