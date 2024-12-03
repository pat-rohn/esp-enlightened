#include "leds_service.h"

#include <string.h>
#include <sstream>
#include <ArduinoJson.h>
#include "mqtt_events.h"

CLEDService::CLEDService(LedStrip *ledStrip) : m_LedStrip(ledStrip)
{
}

String CLEDService::apply(String ledString)
{
  Serial.printf("apply %s\n", ledString.c_str());
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, ledString);
  if (err.code() != DeserializationError::Code::Ok)
  {
    Serial.printf("Failed to parse (%d): %s \n", err.code(), ledString.c_str());
    doc.clear();
    std::stringstream str;
    std::array<uint8_t, 3> color = m_LedStrip->getColor();
    str << R"({"Red":)" << int(color[0])
        << R"( ,"Green": )" << int(color[1])
        << R"( ,"Blue": )" << int(color[2])
        << R"( ,"Brightness": )" << int(m_LedStrip->m_Factor * 100.0)
        << R"( ,"Mode": )" << int(m_LedStrip->m_LEDMode)
        << R"( ,"Message": "Failed to parse: )" << err.code() << R"( ")"
        << R"( })" << std::endl;
  }
  else
  {
    int mode = doc["Mode"];
    m_LedStrip->m_LEDMode = LedStrip::LEDModes(mode);
    double factor = doc["Brightness"];
    m_LedStrip->m_Factor = factor / 100.0;
    int factorRed = doc["Red"];
    int factorGreen = doc["Green"];
    int factorBlue = doc["Blue"];
    String message = doc["Message"];
    m_LedStrip->setColor(factorRed, factorGreen, factorBlue);
    m_LedStrip->applyModeAndColor();
    std::stringstream str;
    std::array<uint8_t, 3> color = m_LedStrip->getColor();
    str << R"({"Red":)" << int(color[0])
        << R"( ,"Green": )" << int(color[1])
        << R"( ,"Blue": )" << int(color[2])
        << R"( ,"Brightness": )" << int(m_LedStrip->m_Factor * 100.0)
        << R"( ,"Mode": )" << int(m_LedStrip->m_LEDMode)
        << R"( ,"Message": "Success - )" << message.c_str() << R"( ")"
        << R"( })" << std::endl;
    doc.clear();
    mqtt_events::sendStateTopic(color, m_LedStrip->m_LEDMode == LedStrip::LEDModes::on, m_LedStrip->m_Factor);
  }

  return get();
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
