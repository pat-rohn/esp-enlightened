#include "leds_service.h"

#include <string.h>
#include <sstream>
#include <ArduinoJson.h>

CLEDService::CLEDService(LedStrip *ledStrip) : m_LedStrip(ledStrip)
{

}

String CLEDService::apply(String ledString)
{
  Serial.println("POST Apply");
  DynamicJsonDocument doc(4096);
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
    if (m_LedStrip->m_LEDMode == LedStrip::LEDModes::pulse)
    {
      m_LedStrip->m_PulseMode.LowerLimit = 0.15;
      m_LedStrip->m_PulseMode.UpperLimit = 0.5;
      m_LedStrip->m_PulseMode.StepSize = 0.002;
      m_LedStrip->m_PulseMode.UpdateInterval = 40;
    }
    double factor = doc["Brightness"];
    m_LedStrip->m_Factor = factor / 100.0;
    int factorRed = doc["Red"];
    int factorGreen = doc["Green"];
    int factorBlue = doc["Blue"];
    String message = doc["Message"];
    m_LedStrip->setColor(factorRed, factorGreen, factorBlue);
    m_LedStrip->apply();
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
  }
  return get();
}

String CLEDService::get()
{
  Serial.println("LED:Service -> GET api");
  std::stringstream str;
  std::array<uint8_t, 3> color = m_LedStrip->getColor();
  str << R"({"Red":)" << int(color[0])
      << R"( ,"Green": )" << int(color[1])
      << R"( ,"Blue": )" << int(color[2])
      << R"( ,"Brightness": )" << int(m_LedStrip->m_Factor * 100)
      << R"( ,"Mode": )" << int(m_LedStrip->m_LEDMode)
      << R"( ,"Message": "Success")"
      << R"( })" << std::endl;
      return String(str.str().c_str());
}
