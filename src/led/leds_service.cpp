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
  if (!doc.is<JsonObject>())
  {
    response = get("Error: body must be a JSON object");
    return false;
  }

  // Partial, like the configuration write: an absent field keeps what the
  // strip is already showing. A brightness slider should not have to restate
  // the colour, and requiring the full set is what made every client hold a
  // complete mirror of the strip's state just to change one number.
  const std::array<uint8_t, 3> current = m_LedStrip->getColor();
  int mode = int(m_LedStrip->m_LEDMode);
  double brightness = m_LedStrip->m_Factor * 100.0;
  int red = current[0];
  int green = current[1];
  int blue = current[2];

  struct Field
  {
    const char *name;
    int *target;
  };
  const Field fields[] = {
      {"Mode", &mode}, {"Red", &red}, {"Green", &green}, {"Blue", &blue}};
  for (const Field &field : fields)
  {
    const JsonVariantConst value = doc[field.name];
    if (value.isNull())
    {
      continue;
    }
    // is<double>() is true for any JSON number and false for strings and
    // bools, so "abc" is refused rather than quietly becoming 0.
    if (!value.is<double>())
    {
      response = get(String("Error: invalid ") + field.name);
      return false;
    }
    *field.target = value.as<int>();
  }

  const JsonVariantConst brightnessValue = doc["Brightness"];
  if (!brightnessValue.isNull())
  {
    if (!brightnessValue.is<double>())
    {
      response = get("Error: invalid Brightness");
      return false;
    }
    brightness = brightnessValue.as<double>();
  }

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

  m_LedStrip->m_Owner = LedStrip::LEDOwner::manual;
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
  // [M13] Build the response with ArduinoJson instead of hand-concatenating
  // strings so a Message containing '"' or '\' can't produce invalid JSON.
  std::array<uint8_t, 3> color = m_LedStrip->getColor();
  JsonDocument doc;
  doc["Red"] = color[0];
  doc["Green"] = color[1];
  doc["Blue"] = color[2];
  doc["Brightness"] = int(m_LedStrip->m_Factor * 100);
  doc["Mode"] = int(m_LedStrip->m_LEDMode);
  doc["Owner"] = LedStrip::ownerName(m_LedStrip->m_Owner);
  doc["Message"] = msg;
  String result;
  serializeJson(doc, result);
  return result;
}
