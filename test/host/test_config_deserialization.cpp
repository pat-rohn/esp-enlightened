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

// A sparse stored file loads against a default-constructed Configuration, so
// every absent field lands on the constructor's default. That is now the only
// source of defaults; the deserializer used to carry a second set of literals
// which had drifted -- LightLow.Red was 32 here and 16 in the constructor.
void test_legacy_config_uses_documented_defaults()
{
  configman::setConfig(configman::Configuration());
  const configman::Configuration result =
      deserializeFixture("legacy-minimal-config.json");

  const configman::Configuration defaults;
  TEST_ASSERT_FALSE(result.IsConfigured);
  TEST_ASSERT_EQUAL_INT(-1, result.DhtPin);
  TEST_ASSERT_EQUAL_INT(-1, result.OneWirePin);
  TEST_ASSERT_EQUAL_INT(-1, result.DeepSleepTime);
  TEST_ASSERT_EQUAL_INT(3, result.BufferedValues);
  TEST_ASSERT_EQUAL_INT(30, result.MeasureInterval);
  TEST_ASSERT_EQUAL_INT(defaults.LightLow.Red, result.LightLow.Red);
  TEST_ASSERT_EQUAL_INT(defaults.LightLow.Green, result.LightLow.Green);
  TEST_ASSERT_EQUAL_INT(defaults.LightLow.Blue, result.LightLow.Blue);
  TEST_ASSERT_EQUAL_INT(8, result.AlarmSettings.DaySettings.at(configman::Monday).AlarmTime.Hours);
  TEST_ASSERT_EQUAL_INT(30, result.AlarmSettings.DaySettings.at(configman::Monday).AlarmTime.Minutes);
}

void test_partial_light_config_defaults_each_omitted_channel()
{
  configman::setConfig(configman::Configuration());
  const configman::Configuration result =
      deserializeFixture("partial-light-config.json");

  const configman::Configuration defaults;
  TEST_ASSERT_EQUAL_INT(99, result.LightLow.Red);
  TEST_ASSERT_EQUAL_INT(defaults.LightLow.Green, result.LightLow.Green);
  TEST_ASSERT_EQUAL_INT(defaults.LightLow.Blue, result.LightLow.Blue);
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

// ArduinoJson 7 yields an empty String for a null variant, and `null` is not
// a string, so a null secret is refused as a type error rather than clearing
// the stored one. Pinned so a library bump cannot turn it into a wipe.
void test_null_secrets_are_treated_as_absent()
{
  configuration::Configuration existing;
  existing.WiFiPassword = "stored-password";
  existing.ApiToken = "";

  const auto result = configuration::deserializeConfig(
      R"({"WiFiPassword":null,"ApiToken":null})", existing);

  TEST_ASSERT_TRUE(result.first);
  TEST_ASSERT_EQUAL_STRING("stored-password", result.second.WiFiPassword.c_str());
  TEST_ASSERT_EQUAL_STRING("", result.second.ApiToken.c_str());
}

// A number must not become the token "42". as<String>() on a numeric variant
// does render digits, so the type check is a genuine guard.
void test_non_string_secrets_are_treated_as_absent()
{
  configuration::Configuration existing;
  existing.ApiToken = "stored-token";

  const auto result = configuration::deserializeConfig(
      R"({"ApiToken":42})", existing);

  TEST_ASSERT_TRUE(result.first);
  TEST_ASSERT_EQUAL_STRING("stored-token", result.second.ApiToken.c_str());
}

// --- Merge semantics -------------------------------------------------------
// Absent means keep. This is the rule the whole client contract rests on: it
// is what lets a browser or the app write one field without carrying the other
// thirty-odd, and it replaces the old "send the complete document" discipline
// that made every client responsible for fields it did not understand.

void test_absent_keys_keep_every_stored_value()
{
  configuration::Configuration existing;
  existing.SensorID = "Schlafzimmer";
  existing.NumberOfLEDs = 39;
  existing.LEDPin = 19;
  existing.MQTTTopic = "/house/bedroom/";
  existing.LightHigh = configuration::Light(1, 2, 3);
  existing.AlarmSettings.SunriseLightTime = 25.0;

  const auto result = configuration::deserializeConfig(
      R"({"SensorID":"Wohnzimmer"})", existing, configuration::ParseMode::Strict);

  TEST_ASSERT_TRUE(result.first);
  TEST_ASSERT_EQUAL_STRING("Wohnzimmer", result.second.SensorID.c_str());
  TEST_ASSERT_EQUAL_INT(39, result.second.NumberOfLEDs);
  TEST_ASSERT_EQUAL_INT(19, result.second.LEDPin);
  TEST_ASSERT_EQUAL_STRING("/house/bedroom/", result.second.MQTTTopic.c_str());
  TEST_ASSERT_EQUAL_INT(1, result.second.LightHigh.Red);
  TEST_ASSERT_EQUAL_INT(2, result.second.LightHigh.Green);
  TEST_ASSERT_EQUAL_INT(3, result.second.LightHigh.Blue);
  TEST_ASSERT_EQUAL_DOUBLE(25.0, result.second.AlarmSettings.SunriseLightTime);
}

// The single most common write the alarm screen makes.
void test_arming_the_alarm_keeps_every_day_schedule()
{
  configuration::Configuration existing;
  existing.AlarmSettings.IsActivated = false;
  existing.AlarmSettings.SunriseLightTime = 20.0;
  existing.AlarmSettings.DaySettings[configuration::Monday].IsActive = true;
  existing.AlarmSettings.DaySettings[configuration::Monday].AlarmTime =
      configuration::Time(6, 10);
  existing.AlarmSettings.DaySettings[configuration::Friday].IsActive = true;
  existing.AlarmSettings.DaySettings[configuration::Friday].AlarmTime =
      configuration::Time(7, 30);

  const auto result = configuration::deserializeConfig(
      R"({"SunriseSettings":{"IsActivated":true}})", existing,
      configuration::ParseMode::Strict);

  TEST_ASSERT_TRUE(result.first);
  const auto &alarm = result.second.AlarmSettings;
  TEST_ASSERT_TRUE(alarm.IsActivated);
  TEST_ASSERT_EQUAL_DOUBLE(20.0, alarm.SunriseLightTime);
  TEST_ASSERT_TRUE(alarm.DaySettings.at(configuration::Monday).IsActive);
  TEST_ASSERT_EQUAL_INT(6, alarm.DaySettings.at(configuration::Monday).AlarmTime.Hours);
  TEST_ASSERT_EQUAL_INT(10, alarm.DaySettings.at(configuration::Monday).AlarmTime.Minutes);
  TEST_ASSERT_EQUAL_INT(7, alarm.DaySettings.at(configuration::Friday).AlarmTime.Hours);
  TEST_ASSERT_EQUAL_INT(30, alarm.DaySettings.at(configuration::Friday).AlarmTime.Minutes);
}

// Toggling one day off must not need its time resent, and must leave the other
// six days alone.
void test_toggling_one_day_keeps_its_time_and_the_other_days()
{
  configuration::Configuration existing;
  existing.AlarmSettings.DaySettings[configuration::Monday].IsActive = true;
  existing.AlarmSettings.DaySettings[configuration::Monday].AlarmTime =
      configuration::Time(6, 10);
  existing.AlarmSettings.DaySettings[configuration::Tuesday].IsActive = true;
  existing.AlarmSettings.DaySettings[configuration::Tuesday].AlarmTime =
      configuration::Time(6, 45);

  const auto result = configuration::deserializeConfig(
      R"({"SunriseSettings":{"Monday":{"IsActive":false}}})", existing,
      configuration::ParseMode::Strict);

  TEST_ASSERT_TRUE(result.first);
  const auto &alarm = result.second.AlarmSettings;
  TEST_ASSERT_FALSE(alarm.DaySettings.at(configuration::Monday).IsActive);
  TEST_ASSERT_EQUAL_INT(6, alarm.DaySettings.at(configuration::Monday).AlarmTime.Hours);
  TEST_ASSERT_EQUAL_INT(10, alarm.DaySettings.at(configuration::Monday).AlarmTime.Minutes);
  TEST_ASSERT_TRUE(alarm.DaySettings.at(configuration::Tuesday).IsActive);
  TEST_ASSERT_EQUAL_INT(45, alarm.DaySettings.at(configuration::Tuesday).AlarmTime.Minutes);
}

// One channel of one preset, which is what "store this colour" sends.
void test_a_single_light_channel_leaves_the_others_alone()
{
  configuration::Configuration existing;
  existing.LightMedium = configuration::Light(64, 32, 4);

  const auto result = configuration::deserializeConfig(
      R"({"LightMedium":{"Green":99}})", existing, configuration::ParseMode::Strict);

  TEST_ASSERT_TRUE(result.first);
  TEST_ASSERT_EQUAL_INT(64, result.second.LightMedium.Red);
  TEST_ASSERT_EQUAL_INT(99, result.second.LightMedium.Green);
  TEST_ASSERT_EQUAL_INT(4, result.second.LightMedium.Blue);
}

// --- Secrets ---------------------------------------------------------------
// Absent keeps, "" clears. The old rule was "blank keeps", which made clearing
// a token impossible without a ClearApiToken escape hatch; omitting secrets
// from redacted output is what makes the new rule safe for GET-then-PUT.

void test_absent_secret_keeps_the_stored_value()
{
  configuration::Configuration existing;
  existing.WiFiPassword = "stored-password";
  existing.ApiToken = "stored-token";

  const auto result = configuration::deserializeConfig(
      R"({"SensorID":"unchanged-otherwise"})", existing,
      configuration::ParseMode::Strict);

  TEST_ASSERT_TRUE(result.first);
  TEST_ASSERT_EQUAL_STRING("stored-password", result.second.WiFiPassword.c_str());
  TEST_ASSERT_EQUAL_STRING("stored-token", result.second.ApiToken.c_str());
}

void test_an_explicit_empty_secret_clears_it()
{
  configuration::Configuration existing;
  existing.ApiToken = "stored-token";
  existing.WiFiPassword = "stored-password";

  const auto result = configuration::deserializeConfig(
      R"({"ApiToken":""})", existing, configuration::ParseMode::Strict);

  TEST_ASSERT_TRUE(result.first);
  TEST_ASSERT_EQUAL_STRING("", result.second.ApiToken.c_str());
  // Clearing the token must not disturb the WiFi credentials.
  TEST_ASSERT_EQUAL_STRING("stored-password", result.second.WiFiPassword.c_str());
}

// The round trip that would destroy a password if secrets were echoed blank.
void test_redacted_output_omits_secrets_so_a_round_trip_keeps_them()
{
  configuration::Configuration existing;
  existing.WiFiPassword = "stored-password";
  existing.ApiToken = "stored-token";

  const String redacted = configuration::serializeConfig(&existing);
  JsonDocument document;
  TEST_ASSERT_TRUE(deserializeJson(document, redacted.c_str()) ==
                   DeserializationError::Ok);
  TEST_ASSERT_FALSE(document["WiFiPassword"].is<const char *>());
  TEST_ASSERT_FALSE(document["ApiToken"].is<const char *>());
  TEST_ASSERT_TRUE(document["HasWiFiPassword"].as<bool>());
  TEST_ASSERT_TRUE(document["HasApiToken"].as<bool>());

  // Feeding the redacted document straight back must preserve both secrets.
  const auto result = configuration::deserializeConfig(
      redacted.c_str(), existing, configuration::ParseMode::Strict);
  TEST_ASSERT_TRUE(result.first);
  TEST_ASSERT_EQUAL_STRING("stored-password", result.second.WiFiPassword.c_str());
  TEST_ASSERT_EQUAL_STRING("stored-token", result.second.ApiToken.c_str());

  // Persistence still writes the real values.
  const String persisted = configuration::serializeConfig(&result.second, true);
  JsonDocument stored;
  TEST_ASSERT_TRUE(deserializeJson(stored, persisted.c_str()) ==
                   DeserializationError::Ok);
  TEST_ASSERT_EQUAL_STRING("stored-password",
                           stored["WiFiPassword"].as<const char *>());
  TEST_ASSERT_EQUAL_STRING("stored-token", stored["ApiToken"].as<const char *>());
}

// --- Type refusal ----------------------------------------------------------

// A strict write is untrusted input: a wrong type is the client's mistake and
// must be reported, not coerced. ArduinoJson would turn "many" into 0.
void test_strict_mode_refuses_a_wrong_typed_field()
{
  configuration::Configuration existing;
  existing.NumberOfLEDs = 39;

  String error;
  const auto result = configuration::deserializeConfig(
      R"({"NumberOfLEDs":"many"})", existing, configuration::ParseMode::Strict,
      &error);

  TEST_ASSERT_FALSE(result.first);
  TEST_ASSERT_TRUE(std::string(error.c_str()).find("NumberOfLEDs") != std::string::npos);
}

// The stored file is already-accepted data, and refusing it makes config.cpp
// overwrite it with defaults -- Wi-Fi credentials included. So a bad field
// keeps its stored value and the document still loads.
void test_lenient_mode_keeps_the_stored_value_for_a_wrong_typed_field()
{
  configuration::Configuration existing;
  existing.NumberOfLEDs = 39;

  const auto result = configuration::deserializeConfig(
      R"({"NumberOfLEDs":"many"})", existing, configuration::ParseMode::Lenient);

  TEST_ASSERT_TRUE(result.first);
  TEST_ASSERT_EQUAL_INT(39, result.second.NumberOfLEDs);
}

void test_a_body_that_is_not_an_object_is_refused()
{
  configuration::Configuration existing;
  String error;
  const auto result = configuration::deserializeConfig(
      R"([1,2,3])", existing, configuration::ParseMode::Strict, &error);

  TEST_ASSERT_FALSE(result.first);
  TEST_ASSERT_TRUE(error.length() > 0);
}

// --- Restart reporting ------------------------------------------------------
// The device already knows which changes it can pick up live; saying so beats
// every client guessing from which field it happened to change.

void test_alarm_and_preset_changes_apply_live()
{
  configuration::Configuration current;
  configuration::Configuration next = current;
  next.AlarmSettings.IsActivated = !current.AlarmSettings.IsActivated;
  next.AlarmSettings.DaySettings[configuration::Monday].AlarmTime =
      configuration::Time(5, 45);
  next.LightHigh = configuration::Light(200, 100, 50);
  next.MeasureInterval = current.MeasureInterval + 5;
  next.Button2GetURL = "http://example.invalid/toggle";

  TEST_ASSERT_FALSE(configuration::requiresRestart(current, next));
}

void test_hardware_and_network_changes_need_a_restart()
{
  const configuration::Configuration current;

  configuration::Configuration pinChange = current;
  pinChange.LEDPin = 21;
  TEST_ASSERT_TRUE(configuration::requiresRestart(current, pinChange));

  configuration::Configuration wifiChange = current;
  wifiChange.WiFiName = "SomewhereElse";
  TEST_ASSERT_TRUE(configuration::requiresRestart(current, wifiChange));

  configuration::Configuration topologyChange = current;
  topologyChange.NumberOfLEDs = 60;
  TEST_ASSERT_TRUE(configuration::requiresRestart(current, topologyChange));

  // The OTA password is the API token, and it is set at boot.
  configuration::Configuration tokenChange = current;
  tokenChange.ApiToken = "fresh-token";
  TEST_ASSERT_TRUE(configuration::requiresRestart(current, tokenChange));
}

void test_an_unchanged_config_needs_no_restart()
{
  const configuration::Configuration current;
  TEST_ASSERT_FALSE(configuration::requiresRestart(current, current));
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
