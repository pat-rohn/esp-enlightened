#include "leds_service.h"

#include <string.h>
#include <sstream>
#include <ArduinoJson.h>
#include "mqtt_events.h"

CLEDService::CLEDService(LedStrip *ledStrip) : m_LedStrip(ledStrip)
{
}

bool CLEDService::apply(const String &ledString, String &response)
{
  Serial.printf("apply %s\n", ledString.c_str());
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, ledString);
  if (err.code() != DeserializationError::Code::Ok)
  {
    Serial.printf("Failed to parse (%d): %s \n", err.code(), ledString.c_str());
    response = get("Error: Failed to parse input");
    return false;
  }

  if (!doc["Mode"].is<int>())
  {
    response = get("Error: invalid or missing Mode");
    return false;
  }

  if (!doc["Brightness"].is<double>() && !doc["Brightness"].is<int>())
  {
    response = get("Error: invalid or missing Brightness");
    return false;
  }

  if (!doc["Red"].is<int>() || !doc["Green"].is<int>() || !doc["Blue"].is<int>())
  {
    response = get("Error: invalid or missing RGB value");
    return false;
  }

  const int mode = doc["Mode"].as<int>();
  const double brightness = doc["Brightness"].as<double>();
  const int red = doc["Red"].as<int>();
  const int green = doc["Green"].as<int>();
  const int blue = doc["Blue"].as<int>();

  if (mode < int(LedStrip::LEDModes::on) || mode > int(LedStrip::LEDModes::pulse))
  {
    response = get("Error: Mode out of range");
    return false;
  }

  if (brightness < 0.0 || brightness > 100.0)
  {
    response = get("Error: Brightness out of range");
    return false;
  }

  if (red < 0 || red > 255 || green < 0 || green > 255 || blue < 0 || blue > 255)
  {
    response = get("Error: RGB value out of range");
    return false;
  }

  String message = "Success";
  if (doc["Message"].is<const char *>())
  {
    message = doc["Message"].as<const char *>();
  }

  m_LedStrip->m_LEDMode = static_cast<LedStrip::LEDModes>(mode);
  m_LedStrip->m_Factor = brightness / 100.0;
  m_LedStrip->setColor(red, green, blue);
  m_LedStrip->applyModeAndColor();

  std::array<uint8_t, 3> color = m_LedStrip->getColor();
  mqtt_events::sendStateTopic(color, m_LedStrip->m_LEDMode == LedStrip::LEDModes::on, m_LedStrip->m_Factor);
  response = get(message == "Success" ? "Success" : "Success - " + message);
  return true;
}

String CLEDService::get(String msg /*= "Success"*/)
{
  Serial.println("LEDs-Service: GET");
  std::stringstream str;
  std::array<uint8_t, 3> color = m_LedStrip->getColor();
  str << R"({"Red":)" << int(color[0])
      << R"( ,"Green": )" << int(color[1])
      << R"( ,"Blue": )" << int(color[2])
      << R"( ,"Brightness": )" << int(m_LedStrip->m_Factor * 100)
      << R"( ,"Mode": )" << int(m_LedStrip->m_LEDMode)
      << R"( ,"Message": ")" << msg.c_str() << R"(")"
      << R"( })" << std::endl;
  return String(str.str().c_str());
}
