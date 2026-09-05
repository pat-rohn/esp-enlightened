#include <ArduinoJson.h>
#include <string>
#include <unity.h>

void test_arduinojson_deserializes_and_serializes_on_host()
{
  JsonDocument document;
  const DeserializationError error =
      deserializeJson(document, R"({"enabled":true,"name":"enlightened"})");

  TEST_ASSERT_TRUE(error == DeserializationError::Ok);
  TEST_ASSERT_TRUE(document["enabled"].as<bool>());
  TEST_ASSERT_EQUAL_STRING("enlightened", document["name"].as<const char *>());

  std::string output;
  serializeJson(document, output);
  TEST_ASSERT_EQUAL_STRING(
      R"({"enabled":true,"name":"enlightened"})", output.c_str());
}
