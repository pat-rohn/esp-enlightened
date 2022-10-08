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

uint8_t kLEDON = 0x0;
uint8_t kLEDOFF = 0x1;

#include "timeseries.h"

#include "config.h"

#include "sensors/sensors.h"
#include "leds_service.h"
#include "webpage.h"

configman::Configuration config = configman::Configuration();
CTimeHelper *timeHelper = new CTimeHelper();

CTimeseries *timeseries;
LedStrip *ledStrip;
CLEDService *ledService;
webpage::CWebPage *webPage;

bool hasSensors = false;

IPAddress local_IP(192, 168, 4, 1);
IPAddress subnet(255, 255, 0, 0);
IPAddress primaryDNS(8, 8, 8, 8);
IPAddress secondaryDNS(8, 8, 4, 4);
IPAddress gateway(192, 168, 4, 1);

bool isAccessPoint = false;

const char *ssidAP = "AI-Caramba";
const char *passwordAP = "ki-caramba";

CTimeseries::Device deviceConfig = CTimeseries::Device("", 60.0, 3);
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
  unsigned long nextWifiLoopTime = millis();
  while (WiFi.status() != WL_CONNECTED && counter <= 15)
  {
    if (millis() > nextWifiLoopTime)
    {
      nextWifiLoopTime = millis() + 1000;
      digitalWrite(LED_BUILTIN, counter % 2 == 0 ? kLEDON : kLEDOFF);
      Serial.print(".");
      delay(400);
      counter++;
      if (counter >= 15)
      {
        return false;
      }
    }
  }
  Serial.println("");
  Serial.print("Connected to WiFi network with IP Address: ");
  digitalWrite(LED_BUILTIN, kLEDOFF);
  Serial.println(WiFi.localIP());
  WiFi.setAutoReconnect(true);
  WiFi.persistent(true);
  return true;
}

void createAccesPoint()
{
  WiFi.disconnect();
  if (!WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS))
  {
    ledService->showError();
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
  if (config.NumberOfLEDs > 0)
  {
    Serial.println("startLedControl");
    ledStrip->beginPixels();
    ledStrip->apply();
    ledStrip->fancy();
    if (hasSensors)
    {
      Serial.println("Pulse Mode");
      ledStrip->m_LEDMode = LedStrip::LEDModes::pulse;
    }
  }
}

void configureDevice()
{
  Serial.println("configureDevice.");
  config = configman::readConfig();
  delete timeseries;
  delete ledStrip;
  delete ledService;
  timeseries = new CTimeseries(config.ServerAddress, timeHelper);
  ledStrip = new LedStrip(config.LEDPin, config.NumberOfLEDs);
  ledService = new CLEDService(ledStrip);
  if (config.ShowWebpage)
  {
    webPage = new webpage::CWebPage();
  }
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

  ledStrip->setColor(red, green, blue);
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
  ledStrip->setColor(red, green, blue);
}

unsigned long lastColorChange = 0;
const double kColorUpdateInterval = 120000;
double co2TestVal = 400;
double tempTestVal = 15;

void colorUpdate()
{
  if (ledStrip->m_LEDMode == LedStrip::LEDModes::pulse && millis() > lastColorChange + kColorUpdateInterval)
  {
    lastColorChange = millis();
    // Serial.print("co2TestVal: ");
    // Serial.println(co2TestVal);
    // setCO2Color(co2TestVal);
    // co2TestVal += 100;
    // tempTestVal += 1.0;
    // setTemperatureColor(tempTestVal);
    // return;

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
        String name = config.SensorID;
        String valueName = name + it->second.name;

        timeseries->addValue(valueName, it->second.value + sensorOffsets[valueName]);
      }
    }
    valueCounter++;
    if (valueCounter >= deviceConfig.Buffer)
    {
      if (WiFi.status() != WL_CONNECTED)
      {
        if (!tryConnect(config.WiFiName.c_str(), config.WiFiPassword.c_str()))
        {
          Serial.println("No WiFi Connection");
          return;
        }
      }
      timeseries->sendData();
      valueCounter = 0;
    }
  }
}

void setup()
{
  Serial.begin(115200);
  Serial.println("setup");
  configman::begin();

  delay(200);
  config = configman::readConfig();
  delay(200);
  if (false) // overwrite showing webpage
  {
    Serial.println("Overwrite to reconfigure (Reset)");
    config.ShowWebpage = true;
    config.IsConfigured = false;
    configman::saveConfig(&config);
    delay(200);
  }

  configureDevice();

  if (!config.IsOfflineMode)
  {
    WiFi.disconnect();
    while (!tryConnect(config.WiFiName.c_str(), config.WiFiPassword.c_str()))
    {
      if (!config.IsConfigured)
      {
        createAccesPoint();
        Serial.println("Created access point");
        break;
      }
      Serial.println("Failed to connect to WiFi. Retry..");
    }
  }
  else
  {
    createAccesPoint();
  }

  pinMode(LED_BUILTIN, OUTPUT);
  if (config.FindSensors && sensor::sensorsInit())
  {
    hasSensors = true;
  }

  startLedControl();
  if (hasSensors)
  {
    String desc = "";
#ifdef ESP8266
    desc += "ESP8266;";
#endif
#ifdef ESP32
    desc += "ESP32;";
#endif
    if (config.IsConfigured)
    {
      desc += sensor::getDescription();
      CTimeseries::DeviceDesc deviceDesc(config.SensorID, desc);
      deviceDesc.Sensors = sensor::getValueNames();
      deviceConfig = timeseries->init(deviceDesc);
      for (auto const &d : deviceConfig.Sensors)
      {
        sensorOffsets[d.Name] = d.Offset;
      }
    }

    lastUpdate = millis() - deviceConfig.Interval;
    if (config.ShowWebpage || !config.IsConfigured)
    {
      webPage->beginServer();
    }
  }
  else
  {
    if (config.ShowWebpage || !config.IsConfigured)
    {
      webPage->beginServer();
    }
    else
    {
      ledService->beginServer();
    }
  }

  digitalWrite(LED_BUILTIN, kLEDOFF);
}

unsigned long nextLoopTime = millis();

void loop()
{
  if (millis() < nextLoopTime)
  {
    return;
  }
  nextLoopTime = millis() + 500;
  if (!config.IsConfigured)
  {
    config = configman::readConfig();
    if (config.IsConfigured)
    {
      configureDevice();
    }
    nextLoopTime = millis() + 30000;
    Serial.print("Connected to device and change configuration: ");
    Serial.println(WiFi.localIP());
    configman::print(&config);
    config = configman::readConfig();
    return;
  }
  if (hasSensors && !isAccessPoint && !config.IsOfflineMode)
  { // should have connection to timeseries server
    measureAndSendSensorData();
  }
  if (hasSensors && config.NumberOfLEDs > 0)
  {
    colorUpdate();
  }
  else
  {
    ledService->listen();
  }
}
