#include <ArduinoJson.h>
#include <string>
#include <unity.h>

#include "Arduino.h"

namespace ArduinoJson
{
  template <>
  struct Converter<String>
  {
    static void toJson(const String &source, JsonVariant destination)
    {
      destination.set(source.c_str());
    }

    static String fromJson(JsonVariantConst source)
    {
      return String(source.as<const char *>());
    }

    static bool checkJson(JsonVariantConst source)
    {
      return source.is<const char *>() || source.isNull();
    }
  };
}

#include "../../../src/config_store.cpp"
#include "../../../src/config.cpp"

namespace
{
  String fixturePath(const char *file)
  {
    return String("test/host/fixtures/") + String(file);
  }

  std::string readFixture(const char *file)
  {
    FILE *fixture = fopen(fixturePath(file).c_str(), "r");
    TEST_ASSERT_NOT_NULL_MESSAGE(fixture, file);

    std::string contents;
    char buffer[256];
    while (fgets(buffer, sizeof(buffer), fixture) != nullptr)
    {
      contents += buffer;
    }
    fclose(fixture);
    return contents;
  }

  configman::Configuration deserializeFixture(const char *file)
  {
    const std::string contents = readFixture(file);
    const auto result = configman::deserializeConfig(contents.c_str());
    TEST_ASSERT_TRUE_MESSAGE(result.first, file);
    return result.second;
  }
}

void test_config_rejects_invalid_json()
{
  const auto result = configman::deserializeConfig("{");
  TEST_ASSERT_FALSE(result.first);
}

void test_legacy_config_uses_documented_defaults()
{
  configman::setConfig(configman::Configuration());
  const configman::Configuration result =
      deserializeFixture("legacy-minimal-config.json");

  TEST_ASSERT_FALSE(result.IsConfigured);
  TEST_ASSERT_EQUAL_INT(-1, result.DhtPin);
  TEST_ASSERT_EQUAL_INT(-1, result.OneWirePin);
  TEST_ASSERT_EQUAL_INT(-1, result.DeepSleepTime);
  TEST_ASSERT_EQUAL_INT(3, result.BufferedValues);
  TEST_ASSERT_EQUAL_INT(30, result.MeasureInterval);
  TEST_ASSERT_EQUAL_INT(32, result.LightLow.Red);
  TEST_ASSERT_EQUAL_INT(4, result.LightLow.Green);
  TEST_ASSERT_EQUAL_INT(0, result.LightLow.Blue);
  TEST_ASSERT_EQUAL_INT(8, result.AlarmSettings.DaySettings.at(configman::Monday).AlarmTime.Hours);
  TEST_ASSERT_EQUAL_INT(30, result.AlarmSettings.DaySettings.at(configman::Monday).AlarmTime.Minutes);
}

void test_partial_light_config_defaults_each_omitted_channel()
{
  configman::setConfig(configman::Configuration());
  const configman::Configuration result =
      deserializeFixture("partial-light-config.json");

  TEST_ASSERT_EQUAL_INT(99, result.LightLow.Red);
  TEST_ASSERT_EQUAL_INT(4, result.LightLow.Green);
  TEST_ASSERT_EQUAL_INT(0, result.LightLow.Blue);
  TEST_ASSERT_EQUAL_INT(64, result.LightMedium.Red);
  TEST_ASSERT_EQUAL_INT(32, result.LightMedium.Green);
  TEST_ASSERT_EQUAL_INT(4, result.LightMedium.Blue);
  TEST_ASSERT_EQUAL_INT(128, result.LightHigh.Red);
  TEST_ASSERT_EQUAL_INT(64, result.LightHigh.Green);
  TEST_ASSERT_EQUAL_INT(24, result.LightHigh.Blue);
}

void test_missing_sunrise_day_uses_alarm_weekday_default()
{
  configman::setConfig(configman::Configuration());
  const auto result = configman::deserializeConfig(
      R"({"IsConfigured":true,"SunriseSettings":{"IsActivated":true,"SunriseLightTime":15,"Monday":{"AlarmTime":"6:45","IsActive":true}}})");

  TEST_ASSERT_TRUE(result.first);
  TEST_ASSERT_TRUE(result.second.AlarmSettings.DaySettings.at(configman::Monday).IsActive);
  TEST_ASSERT_EQUAL_INT(6, result.second.AlarmSettings.DaySettings.at(configman::Monday).AlarmTime.Hours);
  TEST_ASSERT_EQUAL_INT(45, result.second.AlarmSettings.DaySettings.at(configman::Monday).AlarmTime.Minutes);
  TEST_ASSERT_FALSE(result.second.AlarmSettings.DaySettings.at(configman::Tuesday).IsActive);
  TEST_ASSERT_EQUAL_INT(8, result.second.AlarmSettings.DaySettings.at(configman::Tuesday).AlarmTime.Hours);
  TEST_ASSERT_EQUAL_INT(30, result.second.AlarmSettings.DaySettings.at(configman::Tuesday).AlarmTime.Minutes);
}

void test_redacted_secrets_preserve_existing_values_and_stay_redacted()
{
  configman::Configuration existing;
  existing.WiFiPassword = "stored-password";
  existing.ApiToken = "stored-token";
  configman::setConfig(existing);

  const auto result = configman::deserializeConfig(
      R"({"IsConfigured":true,"WiFiPassword":"","ApiToken":""})");
  TEST_ASSERT_TRUE(result.first);
  TEST_ASSERT_EQUAL_STRING("stored-password", result.second.WiFiPassword.c_str());
  TEST_ASSERT_EQUAL_STRING("stored-token", result.second.ApiToken.c_str());

  const String response = configman::serializeConfig(&result.second);
  JsonDocument responseDocument;
  TEST_ASSERT_TRUE(deserializeJson(responseDocument, response.c_str()) ==
                   DeserializationError::Ok);
  TEST_ASSERT_EQUAL_STRING("", responseDocument["WiFiPassword"].as<const char *>());
  TEST_ASSERT_TRUE(responseDocument["HasWiFiPassword"].as<bool>());
  TEST_ASSERT_EQUAL_STRING("", responseDocument["ApiToken"].as<const char *>());
  TEST_ASSERT_TRUE(responseDocument["HasApiToken"].as<bool>());
}
