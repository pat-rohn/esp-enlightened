#include <Arduino.h>
#include <array>
#include <map>
#include <memory>
#include <time.h>
#include <utility>
#include "events.h"
#include "ArduinoMqttClient.h"
#include "handle_buttons.h"
#include "logging.h"
#include "chip_info.h"
#include "version.h"

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
#include <esp_wifi.h>

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

namespace
{
  // Arduino's ESP32 toolchain still defaults to C++11, which predates
  // std::make_unique.
  template <typename T, typename... Args>
  std::unique_ptr<T> make_unique(Args &&...args)
  {
    return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
  }
}

std::unique_ptr<CTimeHelper> timeHelper = make_unique<CTimeHelper>();

std::unique_ptr<timeseries::CTimeseries> timeSeries;
std::unique_ptr<LedStrip> ledStrip;
std::unique_ptr<CLEDService> ledService;
std::unique_ptr<webpage::CWebPage> webPage;
std::unique_ptr<sunrise::CSunriseAlarm> sunriseAlarm;
std::unique_ptr<logging::CLogger> logger;

struct RuntimeCommands
{
  std::atomic<bool> restartTriggered{false};
  std::atomic<bool> buttonPressed1{false};
  std::atomic<bool> buttonPressed2{false};
};

RuntimeCommands runtimeCommands;

WiFiClient wifiClient;
std::unique_ptr<MqttClient> mqttClient;

bool hasSensors = false;

IPAddress local_IP(192, 168, 4, 1);
IPAddress subnet(255, 255, 0, 0);
IPAddress primaryDNS(8, 8, 8, 8);
IPAddress secondaryDNS(8, 8, 4, 4);
IPAddress gateway(192, 168, 4, 1);

bool isAccessPoint = false;

// [M2] Pick the address that actually applies to the current mode: station
// IP is meaningless in AP mode (reports 0.0.0.0) and vice versa.
IPAddress currentIP()
{
  return isAccessPoint ? WiFi.softAPIP() : WiFi.localIP();
}

std::map<String, float> sensorOffsets;

unsigned long lastUpdate = millis();
int valueCounter = 0;

bool tryConnect(std::string ssid, std::string password)
{
  if (ssid.empty() || ssid.c_str() == nullptr || ssid == "null")
  {
    Serial.println("No WiFi configured ");
    logger->m_IsOnline = false;
    return false;
  }
  Serial.printf("Try connecting to: %s\n", ssid.c_str());

#ifdef ESP8266
  WiFi.hostname(configman::getConfig().SensorID);
#endif /* ESP8266 */
#ifdef ESP32

  WiFi.setHostname(configman::getConfig().SensorID.c_str());
#endif /* ESP32 */

  WiFi.begin(ssid.c_str(), password.c_str());

  unsigned long startTime = millis();
  unsigned long nextWifiLoopTime = millis();
  while (WiFi.status() != WL_CONNECTED)
  {
    if (millis() - nextWifiLoopTime >= 1000)
    {
      Serial.printf("Waiting for connection: %s\n", ssid.c_str());
      nextWifiLoopTime = millis();
      digitalWrite(LED_BUILTIN, ledState ? kLEDON : kLEDOFF);
      ledState = !ledState;

      delay(500);
    }

    if (millis() - startTime >= 30000)
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
#ifdef ESP32
  if (configman::getConfig().DeepSleepTime > 0)
  {
    esp_wifi_set_ps(WIFI_PS_MIN_MODEM);
  }
#endif
  logger->m_IsOnline = true;
  return true;
}

void createAccesPoint()
{
  WiFi.disconnect();
  if (!WiFi.mode(WIFI_AP))
  {
    Serial.print("mode failed.");
  }
  // [M2] AP-mode addressing must use the AP-specific API, not the
  // station-mode WiFi.config(): softAPConfig() sets the address the AP
  // actually serves, and WiFi.localIP() would report 0.0.0.0 here.
  if (!WiFi.softAPConfig(local_IP, gateway, subnet))
  {
    Serial.println("AP failed to configure");
    ESP.restart();
  }
  if (!WiFi.softAP(configman::getConfig().WiFiName.c_str(), configman::getConfig().WiFiPassword.c_str()))
  {
    Serial.print("softAP failed.");
    ESP.restart();
  }
  // Only mark the AP as up once softAP() actually succeeded.
  isAccessPoint = true;
  Serial.print("Has access point:");
  Serial.println(WiFi.softAPIP());
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
  // Routes retain non-owning service pointers, so destroy the server before
  // replacing the services it references.
  webPage.reset();
  timeSeries.reset();
  sunriseAlarm.reset();
  ledService.reset();
  ledStrip.reset();
  mqttClient.reset();
  mqttClient = make_unique<MqttClient>(wifiClient);

  if (!configman::getConfig().IsOfflineMode)
  {
    connectToMqtt();
    mqtt_events::setup(mqttClient.get(), configman::getConfig().MQTTTopic);
    if (configman::getConfig().UseMQTT)
    {
      timeSeries = make_unique<ts_mqtt::CTimeseriesMQTT>(configman::getConfig().MQTTTopic, configman::getConfig().ServerAddress, timeHelper.get(), mqttClient.get());
    }
    else
    {
      timeSeries = make_unique<ts_http::CTimeseriesHttp>(configman::getConfig().ServerAddress, timeHelper.get());
    }
  }

  ledStrip = make_unique<LedStrip>(configman::getConfig().LEDPin, configman::getConfig().NumberOfLEDs);
  ledService = make_unique<CLEDService>(ledStrip.get());
  // always created so alarm settings can be activated at runtime via config save
  sunriseAlarm = make_unique<sunrise::CSunriseAlarm>(ledStrip.get(), timeHelper.get());
  sunriseAlarm->applySettings(configman::getConfig().AlarmSettings);
  if (configman::getConfig().AlarmSettings.IsActivated)
  {
    Serial.println("Sunrise Activated");
  }
  button_inputs::button1.pin = configman::getConfig().Button1;
  button_inputs::button2.pin = configman::getConfig().Button2;
  button_inputs::start();
  webPage = make_unique<webpage::CWebPage>();
  runtimeCommands.restartTriggered.store(false);
  runtimeCommands.buttonPressed1.store(false);
  runtimeCommands.buttonPressed2.store(false);
  webPage->setLEDService(ledService.get());
  webPage->setTimeHelper(timeHelper.get());
  webPage->setTriggerFlag(&runtimeCommands.restartTriggered);
  webPage->setButtonsPressed(&runtimeCommands.buttonPressed1, &runtimeCommands.buttonPressed2);
}

unsigned long lastColorChange = 0;
double co2TestVal = 400;
double tempTestVal = 15;

String getDeepSleepWakeMessage(unsigned long deepSleepSeconds)
{
  time_t now = time(nullptr);
  if (now < 1651000000)
  {
    return "Going to sleep";
  }

  time_t wakeTime = now + deepSleepSeconds;
  struct tm wakeTimeInfo;
  localtime_r(&wakeTime, &wakeTimeInfo);

  char wakeTimeBuffer[64];
  strftime(wakeTimeBuffer, sizeof(wakeTimeBuffer), "Going to sleep until %Y-%m-%d %H:%M:%S %Z", &wakeTimeInfo);
  return String(wakeTimeBuffer);
}

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
  if (millis() - lastUpdate < (unsigned long)configman::getConfig().MeasureInterval * 1000)
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
  if (configman::getConfig().IsOfflineMode)
  {
    return true;
  }
  for (it = values.begin(); it != values.end(); it++)
  {
    if (it->second.isValid && timeSeries != nullptr)
    {
      String name = configman::getConfig().SensorID;
      String valueName = name + it->second.name;
      timeSeries->newValue(valueName, it->second.value + sensorOffsets[valueName]);
    }
  }

  valueCounter++;
  if (valueCounter >= configman::getConfig().BufferedValues)
  {
    if (timeSeries != nullptr)
    {
      timeSeries->sendData();
    }
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
  logger = make_unique<logging::CLogger>(configman::getConfig().ServerAddress, configman::getConfig().SensorID);

  logger->m_IsOnline = !configman::getConfig().IsOfflineMode;

  pinMode(LED_BUILTIN, OUTPUT);

  if (!configman::getConfig().IsOfflineMode)
  {
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

  String desc = "";
#ifdef ESP8266
  desc += "ESP8266;";
#endif
#ifdef ESP32
  desc += "ESP32;";
#endif
  desc += "fw:" + getFirmwareVersion();
  desc += ";ip:" + currentIP().toString();

  if (hasSensors)
  {
    desc += ";" + sensor::getDescription();
    lastUpdate = millis() - configman::getConfig().MeasureInterval;
  }

  if (configman::getConfig().IsConfigured)
  {
    Serial.println("Device Information:");
    timeseries::DeviceDesc deviceDesc(configman::getConfig().SensorID, desc);
    if (hasSensors)
    {
      deviceDesc.Sensors = sensor::getSensorNames();
    }
    if (timeSeries != nullptr)
    {
      timeSeries->initDevice(deviceDesc);
    }
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
  Serial.println(currentIP());
  String initMsg = "IoT Device setup: ";
  initMsg += getChipInfo();
  initMsg += " - FW: " + getFirmwareVersion();
  initMsg += " - IP: " + currentIP().toString();
  if (hasSensors)
  {
    initMsg += " - Descr: " + desc;
  }
  logger->logMessage(initMsg);

#ifdef ESP32
  if (configman::getConfig().DeepSleepTime > 0)
  {
    uint64_t deepSleepInUS = static_cast<uint64_t>(configman::getConfig().DeepSleepTime) * 1000000ULL;
    Serial.printf("Set deepsleep to %d seconds\n", configman::getConfig().DeepSleepTime);
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
  // Apply a staged config before handling the restart flag, so a config
  // POSTed together with a restart request is persisted first.
  if (configman::applyStagedConfig())
  {
    Serial.println("----------------------------- CONFIG CHANGED -----------------------------\n\n");
    if (sunriseAlarm != nullptr)
    {
      sunriseAlarm->applySettings(configman::getConfig().AlarmSettings);
    }
  }
  if (runtimeCommands.restartTriggered.load())
  {
    Serial.println("----------------------------- RESTART -----------------------------\n\n");

#ifdef ESP32
    vTaskDelay(pdMS_TO_TICKS(50));
#endif
    ESP.restart();
  }
  if (runtimeCommands.buttonPressed1.load())
  {
    Serial.println("----------------------------- BUTTON1 PRESSED -----------------------------\n\n");
    handleButton1(sunriseAlarm.get(), ledStrip.get());
    runtimeCommands.buttonPressed1.store(false);
  }
  if (runtimeCommands.buttonPressed2.load())
  {
    Serial.println("----------------------------- BUTTON2 PRESSED -----------------------------\n\n");
    handleButton2();
    runtimeCommands.buttonPressed2.store(false);
  }
}

unsigned long lastLoopTime = 0;
unsigned long loopTime = 500;
unsigned long nextInterval = 500;

void loop()
{
#ifdef ESP32
  // if button is pressed, the watchdog seems not to be triggered in the empty loop anymore.
  // maybe the related to this? https://github.com/espressif/arduino-esp32/issues/2493
  vTaskDelay(pdMS_TO_TICKS(1));
#endif
  handleButtons(sunriseAlarm.get(), ledStrip.get());
  handleMQTT();

  if (millis() - lastLoopTime < nextInterval)
  {
    return;
  }
  lastLoopTime = millis();
  nextInterval = loopTime;
  if (configman::getConfig().FindSensors && !hasSensors)
  {
    hasSensors = sensor::sensorsInit(
        configman::getConfig().SerialRX, configman::getConfig().SerialTX,
        configman::getConfig().OneWirePin);
    if (!hasSensors && logger != nullptr)
    {
      logger->logMessage("No sensors found");
      nextInterval += 10000;
    }
  }
  // Must run before the !IsConfigured early return: during first-time setup
  // via the access point, config saves and /restart set flags that would
  // otherwise never be consumed.
  checkWebpageTriggers();
  if (!configman::getConfig().IsConfigured)
  {
    nextInterval = 15000;
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
    Serial.println(currentIP());
    // [D5] Do not dump the full config (WiFi password included) to Serial on
    // every tick; the AP-credentials hint above is enough to get connected.
    Serial.println("--------------------------------");
    return;
  }
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
        String sleepMessage = getDeepSleepWakeMessage(configman::getConfig().DeepSleepTime);
        Serial.printf("%s (%d seconds)\n",
                      sleepMessage.c_str(),
                      configman::getConfig().DeepSleepTime);
        if (logger != nullptr)
        {
          logger->logMessage(sleepMessage + " (" + String(configman::getConfig().DeepSleepTime) + " seconds)");
        }
        if (mqttClient != nullptr && mqttClient->connected())
        {
          mqttClient->stop();
        }
        WiFi.disconnect(true, false);
        WiFi.mode(WIFI_OFF);
        Serial.flush();
        esp_deep_sleep_start();
      }
#endif
    }
  }

  if (configman::getConfig().AlarmSettings.IsActivated && sunriseAlarm != nullptr)
  {
    sunriseAlarm->run();
    // Serial.println(ESP.getFreeHeap());
  }
  else
  {
    loopTime = ledStrip->runModeAction();
  }
}
