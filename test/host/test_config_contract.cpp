#include <ArduinoJson.h>
#include <fstream>
#include <sstream>
#include <string>
#include <unity.h>

void test_arduinojson_deserializes_and_serializes_on_host();
void test_config_rejects_invalid_json();
void test_legacy_config_uses_documented_defaults();
void test_partial_light_config_defaults_each_omitted_channel();
void test_missing_sunrise_day_uses_alarm_weekday_default();
void test_redacted_secrets_preserve_existing_values_and_stay_redacted();
void test_domain_codec_uses_supplied_secret_values();

namespace
{
  std::string readFixture(const char *path)
  {
    std::ifstream file(path);
    std::stringstream contents;
    contents << file.rdbuf();
    return contents.str();
  }

  JsonDocument parseFixture(const char *path)
  {
    JsonDocument document;
    const std::string contents = readFixture(path);
    TEST_ASSERT_FALSE_MESSAGE(contents.empty(), path);
    TEST_ASSERT_TRUE(deserializeJson(document, contents) == DeserializationError::Ok);
    return document;
  }
}

void test_current_config_fixture_has_the_complete_public_contract()
{
  JsonDocument document =
      parseFixture("test/host/fixtures/current-redacted-config.json");
  const char *requiredFields[] = {
      "IsConfigured", "ServerAddress", "SensorID", "WiFiName",
      "WiFiPassword", "HasWiFiPassword", "ApiToken", "HasApiToken",
      "DhtPin", "SerialRX", "SerialTX", "AnalogSensorPin0",
      "AnalogSensorPin1", "WindSensorPin", "RainfallSensorPin", "LEDPin",
      "OneWirePin", "Button1", "Button2", "Button2GetURL", "NumberOfLEDs",
      "FindSensors", "IsOfflineMode", "ShowWebpage", "UseMQTT", "MQTTTopic",
      "MQTTPort", "DeepSleepTime", "BufferedValues", "MeasureInterval",
      "SunriseSettings", "LightLow", "LightMedium", "LightHigh",
  };

  for (const char *field : requiredFields)
  {
    TEST_ASSERT_FALSE_MESSAGE(document[field].isNull(), field);
  }
  TEST_ASSERT_EQUAL_STRING("", document["WiFiPassword"].as<const char *>());
  TEST_ASSERT_TRUE(document["HasWiFiPassword"].as<bool>());
  TEST_ASSERT_EQUAL_STRING("", document["ApiToken"].as<const char *>());
  TEST_ASSERT_FALSE(document["HasApiToken"].as<bool>());
}

void test_legacy_minimal_config_fixture_remains_a_valid_document()
{
  JsonDocument document =
      parseFixture("test/host/fixtures/legacy-minimal-config.json");

  TEST_ASSERT_FALSE(document["IsConfigured"].as<bool>());
  TEST_ASSERT_TRUE(document["LightLow"].isNull());
  TEST_ASSERT_TRUE(document["SunriseSettings"].isNull());
}

void test_partial_light_fixture_preserves_the_m6_regression_case()
{
  JsonDocument document =
      parseFixture("test/host/fixtures/partial-light-config.json");

  TEST_ASSERT_EQUAL_INT(99, document["LightLow"]["Red"].as<int>());
  TEST_ASSERT_TRUE(document["LightLow"]["Green"].isNull());
  TEST_ASSERT_TRUE(document["LightMedium"].isNull());
  TEST_ASSERT_TRUE(document["LightHigh"].isNull());
}

int main()
{
  UNITY_BEGIN();
  RUN_TEST(test_arduinojson_deserializes_and_serializes_on_host);
  RUN_TEST(test_config_rejects_invalid_json);
  RUN_TEST(test_legacy_config_uses_documented_defaults);
  RUN_TEST(test_partial_light_config_defaults_each_omitted_channel);
  RUN_TEST(test_missing_sunrise_day_uses_alarm_weekday_default);
  RUN_TEST(test_redacted_secrets_preserve_existing_values_and_stay_redacted);
  RUN_TEST(test_domain_codec_uses_supplied_secret_values);
  RUN_TEST(test_current_config_fixture_has_the_complete_public_contract);
  RUN_TEST(test_legacy_minimal_config_fixture_remains_a_valid_document);
  RUN_TEST(test_partial_light_fixture_preserves_the_m6_regression_case);
  return UNITY_END();
}
