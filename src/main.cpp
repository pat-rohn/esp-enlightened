#include <Arduino.h>
#include <array>
#include <map>
#include "events.h"

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

#include "timeseries/ts_http.h"
#include "timeseries/ts_mqtt.h"

#include "config.h"

#include "sensors/sensors.h"
#include "led/leds_service.h"
#include "webpage.h"
#include "ledcontrol.h"

configman::Configuration config = configman::Configuration();
CTimeHelper *timeHelper = new CTimeHelper();

timeseries::CTimeseries *timeSeries;
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

timeseries::Device deviceConfig = timeseries::Device("", 60.0, 3);
std::map<String, float> sensorOffsets;

unsigned long lastUpdate = millis();
int valueCounter = 0;

bool tryConnect(std::string ssid, std::string password)
{
  if (ssid.empty() || ssid.c_str() == nullptr || ssid == "null")
  {
    Serial.println("No WiFi configured ");
    return false;
  }
  Serial.printf("Try connecting to: %s\n", ssid.c_str());

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
  Serial.print("\nConnected to WiFi network with IP Address: ");
  Serial.println(WiFi.localIP());
  digitalWrite(LED_BUILTIN, kLEDOFF);
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
    if (hasSensors && !config.IsSunriseAlarm)
    {
      Serial.println("Pulse Mode");
      ledStrip->m_LEDMode = LedStrip::LEDModes::pulse;
    }
    if (config.IsSunriseAlarm)
    {
      Serial.println("Is Alarm Clock");
      ledStrip->m_LEDMode = LedStrip::LEDModes::off;
      ledStrip->apply();
    }
  }
}

void configureDevice()
{
  Serial.println("configureDevice.");
  config = configman::readConfig();
  delete timeSeries;
  delete ledStrip;
  delete ledService;
  timeSeries = new ts_http::CTimeseriesHttp(config.ServerAddress, timeHelper);
  ledStrip = new LedStrip(config.LEDPin, config.NumberOfLEDs);
  ledService = new CLEDService(ledStrip);
  if (config.ShowWebpage)
  {
    webPage = new webpage::CWebPage();
  }
}

unsigned long lastColorChange = 0;
const long kColorUpdateInterval = 120000;
double co2TestVal = 400;
double tempTestVal = 15;

void colorUpdate(const std::map<String, sensor::SensorData> &values)
{
  if (ledStrip->m_LEDMode == LedStrip::LEDModes::pulse &&
      millis() > lastColorChange + kColorUpdateInterval)
  {
    lastColorChange = millis();
    // Serial.print("co2TestVal: ");
    // Serial.println(co2TestVal);
    // setCO2Color(co2TestVal);
    // co2TestVal += 100;
    // tempTestVal += 1.0;
    // setTemperatureColor(tempTestVal);
    // return;

    if (values.empty())
    {
      Serial.println("No Sensors");
      return;
    }
    if (values.count("CO2") && values.at("CO2").isValid)
    {
      ledStrip->setCO2Color(values.at("CO2").value);
    }
    else if (values.count("Temperature"))
    {
      ledStrip->setTemperatureColor(values.at("Temperature").value);
    }
  }
}

void triggerEvents(const std::map<String, sensor::SensorData> &values)
{
  colorUpdate(values);
  if (values.count("WindSpeed"))
  {
    Serial.println("Check for windspeed");
    if (values.at("WindSpeed").value > 4.0)
    {
      Serial.println("Windspeed high, trigger event");
      CallEvent();
    }
  }
}

void measureAndSendSensorData()
{
  if (millis() > lastUpdate + deviceConfig.Interval * 1000)
  {
    lastUpdate = millis();
    auto values = sensor::getValues();

    if (config.NumberOfLEDs > 0 && !config.IsSunriseAlarm)
    {
      colorUpdate(values);
    }

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

        timeSeries->addValue(valueName, it->second.value + sensorOffsets[valueName]);
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
      timeSeries->sendData();
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
    config.WiFiName = "SSID Name";
    config.WiFiPassword = "***";
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
      timeseries::DeviceDesc deviceDesc(config.SensorID, desc);
      deviceDesc.Sensors = sensor::getValueNames();
      auto device = timeSeries->init(deviceDesc);

      Serial.printf("\nDevice Name: %s\n Sensor Offsets:\n", device.Name.c_str());
      deviceConfig = device;
      for (auto const &s : deviceConfig.Sensors)
      {
        Serial.printf("%s \n", s.Name.c_str());
        sensorOffsets[s.Name] = s.Offset;
        Serial.printf(" is %f\n", sensorOffsets[s.Name]);
      }
    }

    lastUpdate = millis() - deviceConfig.Interval;
  }
  else
  {
    if (!config.ShowWebpage && config.IsConfigured)
    {
      ledService->beginServer();
    }
  }
  if (config.ShowWebpage || !config.IsConfigured)
  {
    webPage->beginServer();
  }

  digitalWrite(LED_BUILTIN, kLEDOFF);
  Serial.println("Succesfully set up");
}

unsigned long nextLoopTime = millis();
bool isAlarmActive = false;
unsigned long alarmStartTime = millis();
unsigned long alarmEndTime = millis();
unsigned long loopTime = 500;

void loop()
{
  if (millis() < nextLoopTime)
  {
    return;
  }
  nextLoopTime = millis() + loopTime;
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
    Serial.println(configman::readConfigAsString());
    config = configman::readConfig();
    return;
  }

  if (config.IsSunriseAlarm)
  {
    auto currentTime = timeHelper->getHoursAndMinutes();
    if ((config.AlarmTime.Hours == currentTime.first &&
         config.AlarmTime.Minutes == currentTime.second)) // ||  true (test alarm)
    {
      if (!isAlarmActive)
      {
        Serial.printf("Activate Sunrise: %ld:%ld \n", currentTime.first, currentTime.second);
        alarmEndTime = millis() + (15*60*1000);
        isAlarmActive = true;
        alarmStartTime = millis();
        ledStrip->m_LEDMode = LedStrip::LEDModes::sunrise;
        ledStrip->m_SunriseStartTime = alarmStartTime;
        ledStrip->runModeAction();
        loopTime = 100;
      }
    }
    if (isAlarmActive)
    {
      ledStrip->runModeAction();
      if (millis() > alarmEndTime)
      {
        Serial.printf("Turn off Sunrise: %ld:%ld ", currentTime.first, currentTime.second);
        isAlarmActive = false;
        ledStrip->m_LEDMode = LedStrip::LEDModes::off;
        loopTime = 500;

        ledStrip->apply();
      }
    }
  }
  if (hasSensors && !isAccessPoint && !config.IsOfflineMode)
  { // should have connection to timeseries server
    measureAndSendSensorData();
    // Serial.printf("Heap %d\n", ESP.getFreeHeap());
  }
  if (!hasSensors)
  {
    ledService->listen();
  }
}
