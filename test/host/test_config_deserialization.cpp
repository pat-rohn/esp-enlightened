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

    // Must mirror ArduinoJson's own ::String converter (ConverterImpl.hpp,
    // ARDUINOJSON_ENABLE_ARDUINO_STRING): a real string is copied, anything
    // else is *serialized*, so null becomes "null" and 42 becomes "42".
    // Routing through as<const char *>() instead yields nullptr -> "" and
    // silently makes every non-string variant look absent on the host, which
    // is how the stored-token-"null" lockout survived a green test suite.
    static String fromJson(JsonVariantConst source)
    {
      JsonString str = source.as<JsonString>();
      if (str)
      {
        return String(str.c_str());
      }
      std::string serialized;
      serializeJson(source, serialized);
      return String(serialized);
    }

    static bool checkJson(JsonVariantConst source)
    {
      return source.is<const char *>() || source.isNull();
    }
  };
}

#include "../../../src/config_store.cpp"
#include "../../../src/domain/configuration_codec.cpp"
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

// A JSON null for a secret counts as "absent" (keep whatever is stored),
// never as a value. This is load-bearing: the companion app's form binding
// passes the raw input event through, so clearing the field sends
// {"ApiToken":null}, and storing that as a literal token would switch the
// auth gate on and lock every client out of every mutating endpoint with
// 401 -- with no way back, since a blank token means "keep existing".
// ArduinoJson 7 does NOT give this for free: as<String>() serializes a null
// variant to the four characters "null", so the codec has to check
// is<const char *>() before accepting a secret (see jsonString()).
void test_null_secrets_are_treated_as_absent()
{
  configuration::Configuration existing;
  existing.WiFiPassword = "stored-password";
  existing.ApiToken = "";

  const auto result = configuration::deserializeConfig(
      R"({"IsConfigured":true,"WiFiPassword":null,"ApiToken":null})", existing);

  TEST_ASSERT_TRUE(result.first);
  TEST_ASSERT_EQUAL_STRING("stored-password", result.second.WiFiPassword.c_str());
  TEST_ASSERT_EQUAL_STRING("", result.second.ApiToken.c_str());
}

// The case that actually locked a device out: a stored config written before
// the ApiToken field existed has no "ApiToken" key at all. An absent key is
// the same null variant as an explicit null, so it serialized to "null" and
// became a 4-character token nobody had ever set -- 401 on every mutating
// endpoint, and that string as the OTA password. legacy-minimal-config.json
// is exactly this document.
void test_absent_secret_keys_are_treated_as_absent()
{
  configuration::Configuration existing;
  existing.WiFiPassword = "stored-password";
  existing.ApiToken = "";

  const auto result = configuration::deserializeConfig(
      R"({"IsConfigured":true})", existing);

  TEST_ASSERT_TRUE(result.first);
  TEST_ASSERT_EQUAL_STRING("stored-password", result.second.WiFiPassword.c_str());
  TEST_ASSERT_EQUAL_STRING("", result.second.ApiToken.c_str());
}

// Same for a non-string scalar: a number must not become the token "42".
void test_non_string_secrets_are_treated_as_absent()
{
  configuration::Configuration existing;
  existing.ApiToken = "stored-token";

  const auto result = configuration::deserializeConfig(
      R"({"IsConfigured":true,"ApiToken":42})", existing);

  TEST_ASSERT_TRUE(result.first);
  TEST_ASSERT_EQUAL_STRING("stored-token", result.second.ApiToken.c_str());
}

// An explicit ClearApiToken command must empty the stored token, which is
// otherwise unreachable: a blank ApiToken means "keep existing", so a stray
// token would gate every mutating endpoint forever.
void test_clear_api_token_command_empties_the_stored_token()
{
  configuration::Configuration existing;
  existing.ApiToken = "stored-token";
  existing.WiFiPassword = "stored-password";

  const auto result = configuration::deserializeConfig(
      R"({"IsConfigured":true,"ApiToken":"","ClearApiToken":true})", existing);

  TEST_ASSERT_TRUE(result.first);
  TEST_ASSERT_EQUAL_STRING("", result.second.ApiToken.c_str());
  // Clearing the token must not disturb the WiFi credentials.
  TEST_ASSERT_EQUAL_STRING("stored-password", result.second.WiFiPassword.c_str());
}

// Absent flag (every existing client) keeps the blank-means-keep behaviour.
void test_absent_clear_flag_keeps_the_stored_token()
{
  configuration::Configuration existing;
  existing.ApiToken = "stored-token";

  const auto result = configuration::deserializeConfig(
      R"({"IsConfigured":true,"ApiToken":""})", existing);

  TEST_ASSERT_TRUE(result.first);
  TEST_ASSERT_EQUAL_STRING("stored-token", result.second.ApiToken.c_str());
}

// A contradictory request must keep auth on rather than silently disable it.
void test_supplied_token_wins_over_the_clear_flag()
{
  configuration::Configuration existing;
  existing.ApiToken = "stored-token";

  const auto result = configuration::deserializeConfig(
      R"({"IsConfigured":true,"ApiToken":"fresh-token","ClearApiToken":true})", existing);

  TEST_ASSERT_TRUE(result.first);
  TEST_ASSERT_EQUAL_STRING("fresh-token", result.second.ApiToken.c_str());
}

void test_domain_codec_uses_supplied_secret_values()
{
  configuration::Configuration existing;
  existing.WiFiPassword = "stored-password";
  existing.ApiToken = "stored-token";

  const auto result = configuration::deserializeConfig(
      R"({"IsConfigured":true,"WiFiPassword":"","ApiToken":""})", existing);

  TEST_ASSERT_TRUE(result.first);
  TEST_ASSERT_EQUAL_STRING("stored-password", result.second.WiFiPassword.c_str());
  TEST_ASSERT_EQUAL_STRING("stored-token", result.second.ApiToken.c_str());

  const String persisted = configuration::serializeConfig(&result.second, true);
  JsonDocument document;
  TEST_ASSERT_TRUE(deserializeJson(document, persisted.c_str()) ==
                   DeserializationError::Ok);
  TEST_ASSERT_EQUAL_STRING("stored-password",
                           document["WiFiPassword"].as<const char *>());
  TEST_ASSERT_EQUAL_STRING("stored-token", document["ApiToken"].as<const char *>());
}
