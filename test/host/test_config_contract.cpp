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
void test_null_secrets_are_treated_as_absent();
void test_non_string_secrets_are_treated_as_absent();
void test_absent_keys_keep_every_stored_value();
void test_arming_the_alarm_keeps_every_day_schedule();
void test_toggling_one_day_keeps_its_time_and_the_other_days();
void test_a_single_light_channel_leaves_the_others_alone();
void test_absent_secret_keeps_the_stored_value();
void test_an_explicit_empty_secret_clears_it();
void test_redacted_output_omits_secrets_so_a_round_trip_keeps_them();
void test_strict_mode_refuses_a_wrong_typed_field();
void test_lenient_mode_keeps_the_stored_value_for_a_wrong_typed_field();
void test_a_body_that_is_not_an_object_is_refused();
void test_alarm_and_preset_changes_apply_live();
void test_hardware_and_network_changes_need_a_restart();
void test_an_unchanged_config_needs_no_restart();
void test_deadline_reached_before_rollover();
void test_deadline_reached_after_millis_rollover();
void test_body_chunk_fitting_the_declared_length_is_kept_whole();
void test_body_split_across_chunks_is_reassembled_completely();
void test_body_longer_than_content_length_is_clamped_to_the_buffer();
void test_body_chunk_starting_past_the_declared_end_is_dropped();
void test_final_body_chunk_overshooting_is_trimmed();
void test_zero_length_body_writes_nothing();
void test_writable_chunk_never_reaches_the_terminating_byte();
void test_alarm_time_is_serialized_zero_padded();
void test_alarm_time_parser_accepts_padded_and_legacy_forms();
void test_alarm_time_parser_refuses_everything_else();
void test_strict_mode_refuses_a_config_with_an_unreadable_alarm_time();
void test_lenient_mode_keeps_the_stored_alarm_time_and_still_loads();
void test_a_valid_alarm_time_survives_a_serialize_parse_round_trip();

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
      "HasWiFiPassword", "HasApiToken",
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
  // Redacted output omits the secrets entirely rather than blanking them, so
  // that feeding it back cannot clear what it could not disclose.
  TEST_ASSERT_TRUE(document["WiFiPassword"].isNull());
  TEST_ASSERT_TRUE(document["HasWiFiPassword"].as<bool>());
  TEST_ASSERT_TRUE(document["ApiToken"].isNull());
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
  RUN_TEST(test_null_secrets_are_treated_as_absent);
  RUN_TEST(test_non_string_secrets_are_treated_as_absent);
  RUN_TEST(test_absent_keys_keep_every_stored_value);
  RUN_TEST(test_arming_the_alarm_keeps_every_day_schedule);
  RUN_TEST(test_toggling_one_day_keeps_its_time_and_the_other_days);
  RUN_TEST(test_a_single_light_channel_leaves_the_others_alone);
  RUN_TEST(test_absent_secret_keeps_the_stored_value);
  RUN_TEST(test_an_explicit_empty_secret_clears_it);
  RUN_TEST(test_redacted_output_omits_secrets_so_a_round_trip_keeps_them);
  RUN_TEST(test_strict_mode_refuses_a_wrong_typed_field);
  RUN_TEST(test_lenient_mode_keeps_the_stored_value_for_a_wrong_typed_field);
  RUN_TEST(test_a_body_that_is_not_an_object_is_refused);
  RUN_TEST(test_alarm_and_preset_changes_apply_live);
  RUN_TEST(test_hardware_and_network_changes_need_a_restart);
  RUN_TEST(test_an_unchanged_config_needs_no_restart);
  RUN_TEST(test_deadline_reached_before_rollover);
  RUN_TEST(test_deadline_reached_after_millis_rollover);
  RUN_TEST(test_body_chunk_fitting_the_declared_length_is_kept_whole);
  RUN_TEST(test_body_split_across_chunks_is_reassembled_completely);
  RUN_TEST(test_body_longer_than_content_length_is_clamped_to_the_buffer);
  RUN_TEST(test_body_chunk_starting_past_the_declared_end_is_dropped);
  RUN_TEST(test_final_body_chunk_overshooting_is_trimmed);
  RUN_TEST(test_zero_length_body_writes_nothing);
  RUN_TEST(test_writable_chunk_never_reaches_the_terminating_byte);
  RUN_TEST(test_alarm_time_is_serialized_zero_padded);
  RUN_TEST(test_alarm_time_parser_accepts_padded_and_legacy_forms);
  RUN_TEST(test_alarm_time_parser_refuses_everything_else);
  RUN_TEST(test_strict_mode_refuses_a_config_with_an_unreadable_alarm_time);
  RUN_TEST(test_lenient_mode_keeps_the_stored_alarm_time_and_still_loads);
  RUN_TEST(test_a_valid_alarm_time_survives_a_serialize_parse_round_trip);
  RUN_TEST(test_current_config_fixture_has_the_complete_public_contract);
  RUN_TEST(test_legacy_minimal_config_fixture_remains_a_valid_document);
  RUN_TEST(test_partial_light_fixture_preserves_the_m6_regression_case);
  return UNITY_END();
}
