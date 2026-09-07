#include "configuration_codec.h"

#include <cstdio>
#include <string>

namespace configuration
{
    namespace
    {
        const char *weekdayName(weekday_t weekday)
        {
            switch (weekday)
            {
            case Monday: return "Monday";
            case Tuesday: return "Tuesday";
            case Wednesday: return "Wednesday";
            case Thursday: return "Thursday";
            case Friday: return "Friday";
            case Saturday: return "Saturday";
            case Sunday: return "Sunday";
            default: return "";
            }
        }
    }

    String formatTimeOfDay(const Time &time)
    {
        char buffer[6];
        snprintf(buffer, sizeof(buffer), "%02d:%02d", time.Hours, time.Minutes);
        return String(buffer);
    }

    bool parseTimeOfDay(const char *text, Time &out)
    {
        if (text == nullptr)
        {
            return false;
        }

        const auto skipBlanks = [](const char *cursor)
        {
            while (*cursor == ' ' || *cursor == '\t')
            {
                ++cursor;
            }
            return cursor;
        };

        // Reads at most two digits, so "006:00" and "6:000" are refused rather
        // than silently truncated by the conversion.
        const auto readField = [](const char **cursor, int *value)
        {
            int digits = 0;
            *value = 0;
            while (**cursor >= '0' && **cursor <= '9')
            {
                if (++digits > 2)
                {
                    return false;
                }
                *value = *value * 10 + (**cursor - '0');
                ++(*cursor);
            }
            return digits > 0;
        };

        const char *cursor = skipBlanks(text);

        int hours = 0;
        if (!readField(&cursor, &hours) || *cursor != ':')
        {
            return false;
        }
        ++cursor;

        int minutes = 0;
        if (!readField(&cursor, &minutes))
        {
            return false;
        }

        // Anything left over -- a second colon, a stray letter -- is a reason to
        // refuse, not something to ignore. `lastIndexOf(':')` used to accept
        // "6:5:7" as 06:07.
        if (*skipBlanks(cursor) != '\0')
        {
            return false;
        }

        if (hours > 23 || minutes > 59)
        {
            return false;
        }

        out = Time(hours, minutes);
        return true;
    }

    String serializeConfig(const Configuration *config, bool revealSecrets)
    {
        Serial.println("Serialize config...");
        JsonDocument doc;
        doc["IsConfigured"] = config->IsConfigured;
        doc["ServerAddress"] = config->ServerAddress;
        doc["SensorID"] = config->SensorID;
        doc["WiFiName"] = config->WiFiName;
        doc["WiFiPassword"] = revealSecrets ? config->WiFiPassword : String("");
        doc["HasWiFiPassword"] = config->WiFiPassword.length() > 0;
        doc["ApiToken"] = revealSecrets ? config->ApiToken : String("");
        doc["HasApiToken"] = config->ApiToken.length() > 0;
        doc["DhtPin"] = config->DhtPin;
        doc["SerialRX"] = config->SerialRX;
        doc["SerialTX"] = config->SerialTX;
        doc["AnalogSensorPin0"] = config->AnalogSensorPin0;
        doc["AnalogSensorPin1"] = config->AnalogSensorPin1;
        doc["WindSensorPin"] = config->WindSensorPin;
        doc["RainfallSensorPin"] = config->RainfallSensorPin;
        doc["LEDPin"] = config->LEDPin;
        doc["OneWirePin"] = config->OneWirePin;
        doc["Button1"] = config->Button1;
        doc["Button2"] = config->Button2;
        doc["Button2GetURL"] = config->Button2GetURL;
        doc["NumberOfLEDs"] = config->NumberOfLEDs;
        doc["FindSensors"] = config->FindSensors;
        doc["IsOfflineMode"] = config->IsOfflineMode;
        doc["ShowWebpage"] = config->ShowWebpage;
        doc["UseMQTT"] = config->UseMQTT;
        doc["MQTTTopic"] = config->MQTTTopic;
        doc["MQTTPort"] = config->MQTTPort;
        doc["DeepSleepTime"] = config->DeepSleepTime;
        doc["BufferedValues"] = config->BufferedValues;
        doc["MeasureInterval"] = config->MeasureInterval;

        doc["SunriseSettings"] = serializeSunrise(&config->AlarmSettings);
        JsonDocument docLightLow;
        docLightLow["Red"] = config->LightLow.Red;
        docLightLow["Green"] = config->LightLow.Green;
        docLightLow["Blue"] = config->LightLow.Blue;
        doc["LightLow"] = docLightLow;

        JsonDocument docLightMedium;
        docLightMedium["Red"] = config->LightMedium.Red;
        docLightMedium["Green"] = config->LightMedium.Green;
        docLightMedium["Blue"] = config->LightMedium.Blue;
        doc["LightMedium"] = docLightMedium;

        JsonDocument docLightHigh;
        docLightHigh["Red"] = config->LightHigh.Red;
        docLightHigh["Green"] = config->LightHigh.Green;
        docLightHigh["Blue"] = config->LightHigh.Blue;
        doc["LightHigh"] = docLightHigh;

        Serial.println("make pretty...");
        size_t needed = measureJsonPretty(doc) + 1;
        Serial.printf("Allocate %d memory\n", needed);
        char *buffer = static_cast<char *>(malloc(needed));
        if (!buffer)
        {
            Serial.printf("Can't allocate so much memory (%d)", needed);
            return String("{}");
        }
        serializeJsonPretty(doc, buffer, needed);
        String result(buffer);
        free(buffer);
        return result;
    }

    String prettyPrintConfig(const char *configStr)
    {
        JsonDocument doc;
        const DeserializationError err = deserializeJson(doc, configStr);
        if (err.code() != DeserializationError::Code::Ok)
        {
            return String(err.code());
        }

        Serial.println("make pretty...");
        const size_t needed = measureJsonPretty(doc) + 1;
        Serial.printf("Allocate %d memory\n", needed);
        char *buffer = static_cast<char *>(malloc(needed));
        if (!buffer)
        {
            Serial.printf("Can't allocate so much memory (%d)", needed);
            return String("{}");
        }
        serializeJsonPretty(doc, buffer, needed);
        String result(buffer);
        free(buffer);
        return result;
    }

    JsonDocument serializeSunrise(const SunriseSettings *config)
    {
        JsonDocument doc;
        doc["IsActivated"] = config->IsActivated;
        doc["SunriseLightTime"] = config->SunriseLightTime;
        for (int weekdayNumber = Monday; weekdayNumber <= Sunday; ++weekdayNumber)
        {
            const weekday_t weekday = static_cast<weekday_t>(weekdayNumber);
            doc[weekdayName(weekday)] = serializeDaySettings(&config->DaySettings.at(weekday));
        }
        return doc;
    }

    JsonDocument serializeDaySettings(const AlarmWeekday *config)
    {
        JsonDocument doc;
        // Zero-padded: an unpadded "6:5" is ambiguous to read, cannot be fed
        // to a client-side time picker, and sorts wrongly as a string.
        doc["AlarmTime"] = formatTimeOfDay(config->AlarmTime);
        doc["IsActive"] = config->IsActive;
        return doc;
    }

    std::pair<bool, Configuration> deserializeConfig(
        const char *configStr, const Configuration &existingConfig,
        ParseMode mode, String *error)
    {
        std::pair<bool, Configuration> result(false, Configuration());
        if (error != nullptr)
        {
            *error = String();
        }
        JsonDocument doc;
        const DeserializationError err = deserializeJson(doc, configStr);
        if (err.code() != DeserializationError::Code::Ok)
        {
            Serial.printf("Deserializing failed %d\n", err.code());
            Serial.print(configStr);
            if (error != nullptr)
            {
                *error = String("Malformed JSON: ") + err.c_str();
            }
            return result;
        }
        if (!doc["IsConfigured"].is<bool>())
        {
            Serial.printf("No valid config %s\n", configStr);
            if (error != nullptr)
            {
                *error = String("Missing or non-boolean IsConfigured");
            }
            return result;
        }

        Configuration &parsed = result.second;
        parsed.IsConfigured = doc["IsConfigured"];
        parsed.ServerAddress = doc["ServerAddress"].as<String>();
        parsed.SensorID = doc["SensorID"].as<String>();
        parsed.WiFiName = doc["WiFiName"].as<String>();

        const String incomingPassword = doc["WiFiPassword"].as<String>();
        parsed.WiFiPassword = incomingPassword.length() > 0
                                  ? incomingPassword
                                  : existingConfig.WiFiPassword;
        // Clearing a token needs an explicit command: a blank ApiToken means
        // "keep the stored one" (GET redacts it, so clients round-trip blanks),
        // which otherwise leaves a stray token gating every mutating endpoint
        // with no way back short of reflashing. A supplied non-empty token
        // still wins over the flag, so a contradictory request keeps auth on
        // rather than silently disabling it.
        const bool clearApiToken = doc["ClearApiToken"] | false;
        const String incomingApiToken = doc["ApiToken"].as<String>();
        parsed.ApiToken = incomingApiToken.length() > 0
                              ? incomingApiToken
                              : clearApiToken
                                    ? String("")
                                    : existingConfig.ApiToken;

        parsed.DhtPin = doc["DhtPin"] | -1;
        const JsonVariant serialRX = doc["SerialRX"];
        if (serialRX.isNull())
        {
            Serial.println("Serial Pins not configured");
            parsed.SerialRX = -1;
            parsed.SerialTX = -1;
        }
        else
        {
            parsed.SerialRX = doc["SerialRX"] | -1;
            parsed.SerialTX = doc["SerialTX"] | -1;
        }

        const JsonVariant analogSensorPin0 = doc["AnalogSensorPin0"];
        parsed.AnalogSensorPin0 = analogSensorPin0.isNull()
                                      ? -1
                                      : doc["AnalogSensorPin0"];
        const JsonVariant analogSensorPin1 = doc["AnalogSensorPin1"];
        parsed.AnalogSensorPin1 = analogSensorPin1.isNull()
                                      ? -1
                                      : doc["AnalogSensorPin1"];
        parsed.WindSensorPin = doc["WindSensorPin"] | -1;
        parsed.RainfallSensorPin = doc["RainfallSensorPin"] | -1;
        parsed.LEDPin = doc["LEDPin"] | -1;

        const JsonVariant oneWire = doc["OneWirePin"];
        if (oneWire.isNull())
        {
            Serial.println("One wire does not exist (yet?)");
            parsed.OneWirePin = -1;
            parsed.DeepSleepTime = -1;
            parsed.BufferedValues = 3;
            parsed.MeasureInterval = 30;
        }
        else
        {
            parsed.OneWirePin = doc["OneWirePin"] | -1;
            parsed.DeepSleepTime = doc["DeepSleepTime"] | -1;
            parsed.BufferedValues = doc["BufferedValues"] | 3;
            parsed.MeasureInterval = doc["MeasureInterval"] | 30;
        }

        const JsonVariant button1 = doc["Button1"];
        if (button1.isNull())
        {
            Serial.println("Button configs do not exist (yet?)");
            parsed.Button1 = -1;
            parsed.Button2 = -1;
        }
        else
        {
            parsed.Button1 = doc["Button1"] | -1;
            parsed.Button2 = doc["Button2"] | -1;
        }

        const JsonVariant button2GetURL = doc["Button2GetURL"];
        if (button2GetURL.isNull())
        {
            parsed.Button2GetURL = "http://192.168.1.125/relay/0?turn=toggle";
            Serial.println("Button 2 get URL set to " + parsed.Button2GetURL);
        }
        else
        {
            parsed.Button2GetURL = doc["Button2GetURL"].as<String>();
        }

        parsed.NumberOfLEDs = doc["NumberOfLEDs"] | -1;
        parsed.FindSensors = doc["FindSensors"] | false;
        parsed.IsOfflineMode = doc["IsOfflineMode"] | true;
        parsed.ShowWebpage = doc["ShowWebpage"] | true;

        const JsonVariant useMQTT = doc["UseMQTT"];
        if (useMQTT.isNull())
        {
            Serial.println("UseMQTT did not exist");
            parsed.UseMQTT = false;
            parsed.MQTTPort = 1883;
            parsed.MQTTTopic = "/myplace/myroom/";
        }
        else
        {
            parsed.UseMQTT = doc["UseMQTT"];
            parsed.MQTTPort = doc["MQTTPort"] | 1883;
            parsed.MQTTTopic = doc["MQTTTopic"].as<String>();
        }

        const JsonVariant sunriseSettings = doc["SunriseSettings"];
        String sunriseError;
        if (!sunriseSettings.isNull())
        {
            parsed.AlarmSettings = deserializeSunrise(
                sunriseSettings, existingConfig.AlarmSettings, mode, &sunriseError);
        }
        else
        {
            Serial.println("Warning: Alarm clock does not exist, using defaults");
            parsed.AlarmSettings = SunriseSettings();
        }

        const JsonVariant lightLow = doc["LightLow"];
        if (lightLow.isNull())
        {
            Serial.println("Light settings do not exist (yet?)");
            parsed.LightLow = Light(32, 4, 0);
            parsed.LightMedium = Light(64, 32, 4);
            parsed.LightHigh = Light(128, 64, 24);
        }
        else
        {
            parsed.LightLow.Red = lightLow["Red"] | 32;
            parsed.LightLow.Green = lightLow["Green"] | 4;
            parsed.LightLow.Blue = lightLow["Blue"] | 0;

            const JsonVariant lightMedium = doc["LightMedium"];
            parsed.LightMedium.Red = lightMedium["Red"] | 64;
            parsed.LightMedium.Green = lightMedium["Green"] | 32;
            parsed.LightMedium.Blue = lightMedium["Blue"] | 4;

            const JsonVariant lightHigh = doc["LightHigh"];
            parsed.LightHigh.Red = lightHigh["Red"] | 128;
            parsed.LightHigh.Green = lightHigh["Green"] | 64;
            parsed.LightHigh.Blue = lightHigh["Blue"] | 24;
        }

        // In strict mode a bad alarm time refuses the document, so the client
        // is told instead of the wrong time being stored. In lenient mode the
        // day kept its previously stored time and the document still loads --
        // refusing it here would make config.cpp replace the whole stored
        // configuration, Wi-Fi credentials included, with defaults.
        if (mode == ParseMode::Strict && !sunriseError.isEmpty())
        {
            Serial.printf("Rejecting config: %s\n", sunriseError.c_str());
            if (error != nullptr)
            {
                *error = sunriseError;
            }
            return result;
        }

        result.first = true;
        Serial.println("Success: Deserialized config");
        return result;
    }

    SunriseSettings deserializeSunrise(JsonVariantConst doc,
                                       const SunriseSettings &existing,
                                       ParseMode mode,
                                       String *error)
    {
        SunriseSettings result;
        result.SunriseLightTime = doc["SunriseLightTime"];
        result.IsActivated = doc["IsActivated"];
        if (result.IsActivated)
        {
            Serial.print("\n Sunrise is activated ");
        }

        for (int weekdayNumber = Monday; weekdayNumber <= Sunday; ++weekdayNumber)
        {
            const weekday_t weekday = static_cast<weekday_t>(weekdayNumber);
            const JsonVariantConst day = doc[weekdayName(weekday)];
            const auto storedDay = existing.DaySettings.find(weekday);
            const AlarmWeekday previous = storedDay == existing.DaySettings.end()
                                              ? AlarmWeekday()
                                              : storedDay->second;
            if (!day.is<JsonObjectConst>())
            {
                result.DaySettings[weekday] = AlarmWeekday();
                continue;
            }
            String dayError;
            result.DaySettings[weekday] =
                deserializeDaySetting(day, previous, mode, &dayError);
            if (!dayError.isEmpty() && error != nullptr && error->isEmpty())
            {
                *error = String(weekdayName(weekday)) + ": " + dayError;
            }
        }
        return result;
    }

    AlarmWeekday deserializeDaySetting(JsonVariantConst doc,
                                       const AlarmWeekday &existing,
                                       ParseMode mode,
                                       String *error)
    {
        AlarmWeekday daySetting;
        daySetting.IsActive = doc["IsActive"];
        // Start from the value already stored, so a field we end up refusing to
        // parse leaves the alarm where the user last set it.
        daySetting.AlarmTime = existing.AlarmTime;

        const String alarmTime = doc["AlarmTime"].as<String>();
        Time parsedTime;
        if (parseTimeOfDay(alarmTime.c_str(), parsedTime))
        {
            daySetting.AlarmTime = parsedTime;
            return daySetting;
        }

        // An alarm time that cannot be read is the one field in this document
        // worth failing over: storing the wrong one means the alarm goes off at
        // the wrong time, or not at all, and nothing tells the user.
        Serial.printf("Invalid alarm time '%s'\n", alarmTime.c_str());
        if (mode == ParseMode::Strict && error != nullptr && error->isEmpty())
        {
            *error = String("Invalid alarm time: ") + alarmTime;
        }
        return daySetting;
    }
}
