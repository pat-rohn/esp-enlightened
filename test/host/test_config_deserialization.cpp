#include <ArduinoJson.h>
#include <cstring>
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
// ArduinoJson 7 yields an empty String for a null variant, which gives the
// behaviour we want; this test pins it so a library bump cannot regress it.
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

// Same for a non-string scalar: a number must not become the token "42".
// as<String>() on a numeric variant does render digits, so this one is a
// genuine guard rather than a restatement of library behaviour.
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

// --- [F1] Alarm time format and validation --------------------------------
// The alarm runs on the device, so a time the firmware cannot read is the
// worst defect in the system: the light comes on at the wrong hour, or never,
// and nothing says so. These pin the parser, the padded output, and the
// deliberate split between refusing an HTTP write and repairing stored data.

void test_alarm_time_is_serialized_zero_padded()
{
  configman::AlarmWeekday day;
  day.AlarmTime = configman::Time(6, 5);
  day.IsActive = true;

  const JsonDocument doc = configuration::serializeDaySettings(&day);

  TEST_ASSERT_EQUAL_STRING("06:05", doc["AlarmTime"].as<const char *>());
}

void test_alarm_time_parser_accepts_padded_and_legacy_forms()
{
  configman::Time parsed;

  // Padded, as the firmware now writes it.
  TEST_ASSERT_TRUE(configuration::parseTimeOfDay("06:05", parsed));
  TEST_ASSERT_EQUAL_INT(6, parsed.Hours);
  TEST_ASSERT_EQUAL_INT(5, parsed.Minutes);

  // Unpadded, as every configuration stored by an older build carries it.
  TEST_ASSERT_TRUE(configuration::parseTimeOfDay("6:5", parsed));
  TEST_ASSERT_EQUAL_INT(6, parsed.Hours);
  TEST_ASSERT_EQUAL_INT(5, parsed.Minutes);

  // Half-padded, and both ends of the range.
  TEST_ASSERT_TRUE(configuration::parseTimeOfDay("6:05", parsed));
  TEST_ASSERT_EQUAL_INT(6, parsed.Hours);
  TEST_ASSERT_EQUAL_INT(5, parsed.Minutes);
  TEST_ASSERT_TRUE(configuration::parseTimeOfDay("0:0", parsed));
  TEST_ASSERT_EQUAL_INT(0, parsed.Hours);
  TEST_ASSERT_EQUAL_INT(0, parsed.Minutes);
  TEST_ASSERT_TRUE(configuration::parseTimeOfDay("23:59", parsed));
  TEST_ASSERT_EQUAL_INT(23, parsed.Hours);
  TEST_ASSERT_EQUAL_INT(59, parsed.Minutes);

  // Surrounding blanks are tolerated, not treated as junk.
  TEST_ASSERT_TRUE(configuration::parseTimeOfDay("  7:30 ", parsed));
  TEST_ASSERT_EQUAL_INT(7, parsed.Hours);
  TEST_ASSERT_EQUAL_INT(30, parsed.Minutes);
}

void test_alarm_time_parser_refuses_everything_else()
{
  configman::Time parsed(9, 9);
  const char *rejected[] = {
      "24:00",   // hour out of range
      "23:60",   // minute out of range
      "-1:00",   // no sign handling; the '-' is junk
      "",        // empty
      "6",       // no separator -- the old parser turned this into 08:30
      "6:",      // no minutes
      ":30",     // no hours
      "6:5:7",   // lastIndexOf(':') used to read this as 06:07
      "ab:cd",   // toInt() used to turn this into 00:00
      "006:00",  // over-long field, silently truncated before
      "6:000",
      "6:5x",    // trailing junk
      nullptr,
  };

  for (const char **candidate = rejected; *candidate != nullptr; ++candidate)
  {
    TEST_ASSERT_FALSE_MESSAGE(configuration::parseTimeOfDay(*candidate, parsed),
                              *candidate);
  }
  // A refused parse leaves the caller's value alone.
  TEST_ASSERT_EQUAL_INT(9, parsed.Hours);
  TEST_ASSERT_EQUAL_INT(9, parsed.Minutes);

  TEST_ASSERT_FALSE(configuration::parseTimeOfDay(nullptr, parsed));
}

void test_strict_mode_refuses_a_config_with_an_unreadable_alarm_time()
{
  configman::Configuration existing;
  String error;
  const auto result = configuration::deserializeConfig(
      R"({"IsConfigured":true,"SunriseSettings":{"IsActivated":true,"Monday":{"AlarmTime":"25:99","IsActive":true}}})",
      existing, configuration::ParseMode::Strict, &error);

  TEST_ASSERT_FALSE(result.first);
  // The message has to name the day and quote the offending value back, so the
  // app can say which row the user needs to fix.
  TEST_ASSERT_NOT_NULL(strstr(error.c_str(), "Monday"));
  TEST_ASSERT_NOT_NULL(strstr(error.c_str(), "25:99"));
}

void test_lenient_mode_keeps_the_stored_alarm_time_and_still_loads()
{
  // Reading the device's own configuration must not fail over one field:
  // config.cpp answers a failed load by overwriting the file with defaults,
  // which would take the Wi-Fi credentials with it.
  configman::Configuration existing;
  existing.AlarmSettings.DaySettings[configman::Monday].AlarmTime =
      configman::Time(6, 10);
  existing.AlarmSettings.DaySettings[configman::Monday].IsActive = true;

  String error;
  const auto result = configuration::deserializeConfig(
      R"({"IsConfigured":true,"SunriseSettings":{"IsActivated":true,"Monday":{"AlarmTime":"nonsense","IsActive":true}}})",
      existing, configuration::ParseMode::Lenient, &error);

  TEST_ASSERT_TRUE(result.first);
  TEST_ASSERT_TRUE(error.isEmpty());
  const auto &monday = result.second.AlarmSettings.DaySettings.at(configman::Monday);
  TEST_ASSERT_EQUAL_INT(6, monday.AlarmTime.Hours);
  TEST_ASSERT_EQUAL_INT(10, monday.AlarmTime.Minutes);
  TEST_ASSERT_TRUE(monday.IsActive);
}

void test_a_valid_alarm_time_survives_a_serialize_parse_round_trip()
{
  configman::Configuration existing;
  configman::AlarmWeekday day;
  day.AlarmTime = configman::Time(6, 5);
  day.IsActive = true;
  existing.AlarmSettings.DaySettings[configman::Monday] = day;

  const String serialized =
      configuration::serializeConfig(&existing, /*revealSecrets=*/true);
  String error;
  const auto reparsed = configuration::deserializeConfig(
      serialized.c_str(), configman::Configuration(),
      configuration::ParseMode::Strict, &error);

  TEST_ASSERT_TRUE_MESSAGE(reparsed.first, error.c_str());
  const auto &monday = reparsed.second.AlarmSettings.DaySettings.at(configman::Monday);
  TEST_ASSERT_EQUAL_INT(6, monday.AlarmTime.Hours);
  TEST_ASSERT_EQUAL_INT(5, monday.AlarmTime.Minutes);
}
