#include <Arduino.h>
#include <array>
#include <map>
#include "events.h"
#include "ArduinoMqttClient.h"
#include "handle_buttons.h"

bool ledState = false;
#ifdef ESP8266
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>

uint8_t kLEDON = 0x0;
uint8_t kLEDOFF = 0x1;

#endif /* ESP8266 */

#ifdef ESP32
#include "WiFi.h"
#include <HTTPClient.h>

uint8_t kLEDON = 0x1;
uint8_t kLEDOFF = 0x0;

#endif /* ESP32 */

#include "timeseries/ts_http.h"
#include "timeseries/ts_mqtt.h"

#include "config.h"

#include "sensors/sensors.h"
#include "led/leds_service.h"
#include "led/button_inputs.h"
#include "webpage.h"
#include "led/sunrise_alarm.h"
#include "mqtt_events.h"

CTimeHelper *timeHelper = new CTimeHelper();

timeseries::CTimeseries *timeSeries;
LedStrip *ledStrip;
CLEDService *ledService;
webpage::CWebPage *webPage;
sunrise::CSunriseAlarm *sunriseAlarm;

// webpage triggers
std::atomic<bool> restartTriggered;
std::atomic<bool> buttonPressed1;
std::atomic<bool> buttonPressed2;

WiFiClient wifiClient;
MqttClient *mqttClient;

bool hasSensors = false;

IPAddress local_IP(192, 168, 4, 1);
IPAddress subnet(255, 255, 0, 0);
IPAddress primaryDNS(8, 8, 8, 8);
IPAddress secondaryDNS(8, 8, 4, 4);
IPAddress gateway(192, 168, 4, 1);

bool isAccessPoint = false;

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

  unsigned long nextWifiLoopTime = millis();
  unsigned long timeoutTime = millis() + 30000;
  while (WiFi.status() != WL_CONNECTED)
  {
    if (millis() > nextWifiLoopTime)
    {
      Serial.printf("Waiting for connection: %s\n", ssid.c_str());
      nextWifiLoopTime = millis() + 1000;
      digitalWrite(LED_BUILTIN, ledState ? kLEDON : kLEDOFF);
      ledState = !ledState;

      delay(500);
    }

    if (millis() > timeoutTime)
    {
      ESP.restart();
    }

    delay(100);
  }
  Serial.print("\nConnected to WiFi network with IP Address: ");
  Serial.println(WiFi.localIP());
  if (configman::getConfig().Button1 < 0)
  {
    digitalWrite(LED_BUILTIN, kLEDOFF);
  }
  WiFi.setAutoReconnect(true);
  WiFi.persistent(true);
  return true;
}

void createAccesPoint()
{
  WiFi.disconnect();
  if (!WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS))
  {
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
  if (!WiFi.softAP(configman::getConfig().WiFiName.c_str(), configman::getConfig().WiFiPassword.c_str()))
  {
    Serial.print("softAP failed.");
    setup();
  }
}

void startLedControl()
{
  if (configman::getConfig().NumberOfLEDs <= 0)
  {
    return;
  }
  Serial.println("startLedControl");
  ledStrip->beginPixels(!configman::getConfig().AlarmSettings.IsActivated);
  if (configman::getConfig().AlarmSettings.IsActivated)
  {
    Serial.println("Is Alarm Clock");
  }
}

void connectToMqtt()
{
  if (mqttClient == nullptr)
  {
    Serial.println("no mqtt client!.");
    return;
  }
  if (configman::getConfig().UseMQTT && !mqttClient->connected())
  {
    if (!WiFi.isConnected())
    {
      Serial.println("MQTT: No WiFi");
      return;
    }
    if (!mqttClient->connected())
    {
      String broker = timeseries::splitAddress(configman::getConfig().ServerAddress, 0);

      Serial.printf("MQTT: Try connecting to %s:%d\n", broker.c_str(), configman::getConfig().MQTTPort);
      if (!mqttClient->connect(broker.c_str(), configman::getConfig().MQTTPort))
      {
        Serial.print("MQTT connection failed! Error code = ");
        Serial.println(mqttClient->connectError());
      }
      else
      {
        Serial.printf("MQTT: Connected to %s:%d\n", broker.c_str(), configman::getConfig().MQTTPort);
      }
    }
  }
}

void configureDevice()
{
  Serial.println("configureDevice");
  delete timeSeries;
  delete ledStrip;
  delete ledService;
  delete sunriseAlarm;
  delete mqttClient;
  mqttClient = new MqttClient(wifiClient);

  String server = timeseries::splitAddress(configman::getConfig().ServerAddress, 0);
  connectToMqtt();
  mqtt_events::setup(mqttClient, configman::getConfig().MQTTTopic);

  if (configman::getConfig().UseMQTT)
  {
    timeSeries = new ts_mqtt::CTimeseriesMQTT(configman::getConfig().MQTTTopic, server, timeHelper, mqttClient);
  }
  else
  {
    timeSeries = new ts_http::CTimeseriesHttp(configman::getConfig().ServerAddress, timeHelper);
  }

  ledStrip = new LedStrip(configman::getConfig().LEDPin, configman::getConfig().NumberOfLEDs);
  ledService = new CLEDService(ledStrip);
  if (configman::getConfig().AlarmSettings.IsActivated)
  {
    Serial.println("Sunrise Activated");
    sunriseAlarm = new sunrise::CSunriseAlarm(ledStrip, timeHelper);
    sunriseAlarm->applySettings(configman::getConfig().AlarmSettings);
  }
  button_inputs::button1.pin = configman::getConfig().Button1;
  button_inputs::button2.pin = configman::getConfig().Button2;
  button_inputs::start();
  webPage = new webpage::CWebPage();
  restartTriggered = new std::atomic<bool>();
  webPage->setLEDService(ledService);
  webPage->setTimeHelper(timeHelper);
  restartTriggered.store(false);
  webPage->setTriggerFlag(&restartTriggered);

  buttonPressed1 = new std::atomic<bool>();
  buttonPressed1.store(false);
  buttonPressed2 = new std::atomic<bool>();
  buttonPressed2.store(false);
  webPage->setButtonsPressed(&buttonPressed1, &buttonPressed2);
}

unsigned long lastColorChange = 0;
double co2TestVal = 400;
double tempTestVal = 15;

void colorUpdate(const std::map<String, sensor::SensorData> &values)
{
  if (ledStrip->m_LEDMode != LedStrip::LEDModes::pulse)
  {
    Serial.printf("Changed to pulse mode. Mode was %d\n", int(ledStrip->m_LEDMode));
    ledStrip->m_LEDMode = LedStrip::LEDModes::pulse;
  }

  // Serial.print("co2TestVal: ");
  // Serial.println(co2TestVal);
  // ledStrip->setCO2Color(co2TestVal);
  // co2TestVal += 100;
  // tempTestVal += 1.0;
  // ledStrip->setTemperatureColor(tempTestVal);
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

void triggerEvents(const std::map<String, sensor::SensorData> &values)
{
  colorUpdate(values);
  if (values.count("WindSpeed"))
  {
    Serial.println("Check for windspeed");
    if (values.at("WindSpeed").value > 4.0)
    {
      Serial.println("TODO: Windspeed high, trigger event");
      CallEvent(configman::getConfig().Button2GetURL);
    }
  }
}

bool measureAndSendSensorData()
{
  if (millis() < lastUpdate + configman::getConfig().MeasureInterval * 1000)
  {
    return false;
  }

  lastUpdate = millis();
  auto values = sensor::getValues();

  if (configman::getConfig().NumberOfLEDs > 0 && !configman::getConfig().AlarmSettings.IsActivated)
  {
    colorUpdate(values);
  }
  else
  {
    digitalWrite(LED_BUILTIN, ledState ? kLEDON : kLEDOFF);
    ledState = !ledState;
  }

  std::vector<String> valueNames;
  std::vector<float> tsValues;
  std::map<String, sensor::SensorData>::iterator it;
  if (values.empty())
  {
    Serial.println("No Values");
    return false;
  }
  for (it = values.begin(); it != values.end(); it++)
  {
    if (it->second.isValid)
    {
      String name = configman::getConfig().SensorID;
      String valueName = name + it->second.name;
      timeSeries->newValue(valueName, it->second.value + sensorOffsets[valueName]);
    }
  }

  valueCounter++;
  if (valueCounter >= configman::getConfig().BufferedValues)
  {
    timeSeries->sendData();
    valueCounter = 0;
    return true;
  }
  return false;
}

void setup()
{
  Serial.begin(115200);
  Serial.println("setup");
  configman::begin();

  if (false)
  {
    Serial.println("Overwrite config to reconfigure (Reset)");
    auto config = configman::Configuration();
    configman::saveConfig(&config);
    delay(200);
  }
  else if (false)
  {
    Serial.println("reset WiFi");
    auto config = configman::readConfig();    
    config.WiFiName = String("Enlightened");
    config.WiFiPassword = String("enlighten-me");
    config.IsOfflineMode = false; // if true it creates access-point
    // config.IsConfigured = false;
    config.ShowWebpage = true;
    configman::saveConfig(&config);
    delay(200);
  }
  else
  {
    configman::readConfig();
  }

  pinMode(LED_BUILTIN, OUTPUT);

  if (!configman::getConfig().IsOfflineMode)
  {
    WiFi.disconnect();
    while (!tryConnect(configman::getConfig().WiFiName.c_str(), configman::getConfig().WiFiPassword.c_str()))
    {
      if (!configman::getConfig().IsConfigured)
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

  configureDevice();
  startLedControl();

  if (configman::getConfig().FindSensors &&
      sensor::sensorsInit(
          configman::getConfig().SerialRX, configman::getConfig().SerialTX,
          configman::getConfig().OneWirePin))
  {
    hasSensors = true;
  }

  if (hasSensors)
  {
    String desc = "";
#ifdef ESP8266
    desc += "ESP8266;";
#endif
#ifdef ESP32
    desc += "ESP32;";
#endif

    if (configman::getConfig().IsConfigured && !configman::getConfig().UseMQTT)
    {
      Serial.println("Device Information:");
      desc += sensor::getDescription();
      timeseries::DeviceDesc deviceDesc(configman::getConfig().SensorID, desc);
    }

    lastUpdate = millis() - configman::getConfig().MeasureInterval;
  }

  webPage->beginServer();

  digitalWrite(LED_BUILTIN, kLEDOFF);
  if (configman::getConfig().NumberOfLEDs > 0)
  {
    ledStrip->m_LEDMode = LedStrip::LEDModes::off;
    ledStrip->m_Factor = 0.5;
    ledStrip->applyModeAndColor();
    ledStrip->setColor(configman::getConfig().LightHigh.Red,
                       configman::getConfig().LightHigh.Green,
                       configman::getConfig().LightHigh.Blue);

    mqtt_events::sendStateTopic(ledStrip->getColor(),
                                ledStrip->m_LEDMode == LedStrip::LEDModes::on,
                                ledStrip->m_Factor);
  }
  Serial.println("Succesfully set up");
  Serial.println(WiFi.localIP());
#ifdef ESP32
  if (configman::getConfig().DeepSleepTime > 0)
  {
    unsigned long deepSleepInUS = configman::getConfig().DeepSleepTime * 1000000;
    Serial.printf("Set deepsleep to %d\n", deepSleepInUS);
    esp_sleep_enable_timer_wakeup(deepSleepInUS);
  }
#else
  Serial.println("Deep sleep not supported.");
#endif
}

void handleMQTT()
{
  if (configman::getConfig().NumberOfLEDs > 0 && mqtt_events::poll())
  {
    std::array<uint8_t, 3> c = mqtt_events::getRGB();
    ledStrip->setColor(c[0], c[1], c[2]);
    if (mqtt_events::getIsOn())
    {
      ledStrip->m_LEDMode = LedStrip::LEDModes::on;
    }
    else
    {
      ledStrip->m_LEDMode = LedStrip::LEDModes::off;
    }
    ledStrip->m_Factor = mqtt_events::getBrightness() / 100.0;
    ledStrip->applyModeAndColor();
    mqtt_events::sendStateTopic(c, ledStrip->m_LEDMode == LedStrip::LEDModes::on, ledStrip->m_Factor);
  }
}

void checkWebpageTriggers()
{
  if (restartTriggered.load())
  {
    Serial.println("----------------------------- RESTART -----------------------------\n\n");

#ifdef ESP32
    vTaskDelay(pdMS_TO_TICKS(50));
#endif
    ESP.restart();
    // restartTriggered.store(false);
  }
  if (buttonPressed1.load())
  {
    Serial.println("----------------------------- BUTTON1 PRESSED -----------------------------\n\n");
    handleButton1(sunriseAlarm, ledStrip);
    buttonPressed1.store(false);
  }
  if (buttonPressed2.load())
  {
    Serial.println("----------------------------- BUTTON2 PRESSED -----------------------------\n\n");
    handleButton2();
    buttonPressed2.store(false);
  }
}

unsigned long nextLoopTime = millis();

unsigned long loopTime = 500;

void loop()
{
#ifdef ESP32
  // if button is pressed, the watchdog seems not to be triggered in the empty loop anymore.
  // maybe the related to this? https://github.com/espressif/arduino-esp32/issues/2493
  vTaskDelay(pdMS_TO_TICKS(1));
#endif
  handleButtons(sunriseAlarm, ledStrip);
  handleMQTT();

  if (millis() < nextLoopTime)
  {
    return;
  }

  nextLoopTime = millis() + loopTime;
  if (!configman::getConfig().IsConfigured)
  {
    nextLoopTime = millis() + 15000;
    Serial.println("Device not configured yet...");
    if (isAccessPoint)
    {
      Serial.print("Connect to access point and configure device:\nWiFiName: ");
      Serial.print(configman::getConfig().WiFiName);
      Serial.print("\nWiFiPassword: ");
      Serial.println(configman::getConfig().WiFiPassword);

      Serial.print("\nSubnet: 16");
      // Serial.println(WiFi.subnetCIDR());
      Serial.print("\ngatewayIP: ");
      Serial.println(WiFi.gatewayIP().toString());
      Serial.print("\nTo stay connected configure static IP (e.g. 192.168.4.5) and use DNS1 0.0.0.0 ");
      Serial.println(WiFi.gatewayIP().toString());
    }
    Serial.print("\nIP Address: ");
    Serial.println(WiFi.localIP());
    auto config = configman::getConfig();
    Serial.println(configman::serializeConfig(&config));
    Serial.println("--------------------------------");
    return;
  }
  checkWebpageTriggers();
  if (!isAccessPoint && !configman::getConfig().IsOfflineMode)
  {
    if (WiFi.status() != WL_CONNECTED)
    {
      if (!tryConnect(configman::getConfig().WiFiName.c_str(), configman::getConfig().WiFiPassword.c_str()))
      {
        Serial.println("No WiFi Connection");
        return;
      }
    }
    if (configman::getConfig().UseMQTT && !mqttClient->connected())
    {
      connectToMqtt();
      mqtt_events::subscribe();
    }
    if (!timeHelper->isTimeSet())
    {
      if (!timeHelper->initTime())
      {
        Serial.println("Time not yet initialized.");
        return;
      }
    }
  }

  if (hasSensors)
  {
    if (measureAndSendSensorData())
    {
#ifdef ESP32
      if (configman::getConfig().DeepSleepTime > 0)
      {
        Serial.printf("Going to sleep for %d",
                      configman::getConfig().DeepSleepTime);
        delay(1000);
        Serial.flush();
        esp_deep_sleep_start();
      }
#endif
    }
  }

  if (configman::getConfig().AlarmSettings.IsActivated)
  {
    sunriseAlarm->run();
    // Serial.println(ESP.getFreeHeap());
  }
  else
  {
    loopTime = ledStrip->runModeAction();
  }
}
