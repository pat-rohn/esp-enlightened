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

        // Records the first refusal reason. Strict callers (HTTP writes) refuse
        // the document; lenient callers (the stored file) keep the old value
        // and carry on, because refusing a stored document makes config.cpp
        // overwrite it with defaults, Wi-Fi credentials included.
        void refuse(ParseMode mode, String *error, const String &reason)
        {
            if (mode == ParseMode::Strict && error != nullptr && error->isEmpty())
            {
                *error = reason;
            }
        }

        // The merge primitives. An absent key keeps the stored value; a key
        // that is present but of the wrong type is a client mistake worth
        // reporting, because ArduinoJson would otherwise quietly turn "abc"
        // into 0 and store it.
        bool pickBool(JsonVariantConst doc, const char *key, bool existing,
                      ParseMode mode, String *error)
        {
            const JsonVariantConst value = doc[key];
            if (value.isNull())
            {
                return existing;
            }
            if (!value.is<bool>())
            {
                refuse(mode, error, String(key) + ": expected true or false");
                return existing;
            }
            return value.as<bool>();
        }

        // is<double>() is true for any JSON number and false for strings and
        // bools, which is exactly the test wanted for an integer field too.
        int pickInt(JsonVariantConst doc, const char *key, int existing,
                    ParseMode mode, String *error)
        {
            const JsonVariantConst value = doc[key];
            if (value.isNull())
            {
                return existing;
            }
            if (!value.is<double>())
            {
                refuse(mode, error, String(key) + ": expected a number");
                return existing;
            }
            return value.as<int>();
        }

        double pickDouble(JsonVariantConst doc, const char *key, double existing,
                          ParseMode mode, String *error)
        {
            const JsonVariantConst value = doc[key];
            if (value.isNull())
            {
                return existing;
            }
            if (!value.is<double>())
            {
                refuse(mode, error, String(key) + ": expected a number");
                return existing;
            }
            return value.as<double>();
        }

        String pickString(JsonVariantConst doc, const char *key, const String &existing,
                          ParseMode mode, String *error)
        {
            const JsonVariantConst value = doc[key];
            if (value.isNull())
            {
                return existing;
            }
            if (!value.is<const char *>())
            {
                refuse(mode, error, String(key) + ": expected a string");
                return existing;
            }
            return value.as<String>();
        }

        Light pickLight(JsonVariantConst doc, const char *key, const Light &existing,
                        ParseMode mode, String *error)
        {
            const JsonVariantConst value = doc[key];
            if (value.isNull())
            {
                return existing;
            }
            if (!value.is<JsonObjectConst>())
            {
                refuse(mode, error, String(key) + ": expected an object");
                return existing;
            }
            Light light = existing;
            light.Red = pickInt(value, "Red", light.Red, mode, error);
            light.Green = pickInt(value, "Green", light.Green, mode, error);
            light.Blue = pickInt(value, "Blue", light.Blue, mode, error);
            return light;
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

    bool requiresRestart(const Configuration &current, const Configuration &next)
    {
        return current.IsConfigured != next.IsConfigured ||
               current.ServerAddress != next.ServerAddress ||
               current.SensorID != next.SensorID ||
               current.WiFiName != next.WiFiName ||
               current.WiFiPassword != next.WiFiPassword ||
               // The OTA password is the API token, and it is set at boot.
               current.ApiToken != next.ApiToken ||
               current.IsOfflineMode != next.IsOfflineMode ||
               current.FindSensors != next.FindSensors ||
               current.DhtPin != next.DhtPin ||
               current.SerialRX != next.SerialRX ||
               current.SerialTX != next.SerialTX ||
               current.AnalogSensorPin0 != next.AnalogSensorPin0 ||
               current.AnalogSensorPin1 != next.AnalogSensorPin1 ||
               current.WindSensorPin != next.WindSensorPin ||
               current.RainfallSensorPin != next.RainfallSensorPin ||
               current.LEDPin != next.LEDPin ||
               current.OneWirePin != next.OneWirePin ||
               current.NumberOfLEDs != next.NumberOfLEDs ||
               current.Button1 != next.Button1 ||
               current.Button2 != next.Button2 ||
               current.UseMQTT != next.UseMQTT ||
               current.MQTTTopic != next.MQTTTopic ||
               current.MQTTPort != next.MQTTPort ||
               current.ShowWebpage != next.ShowWebpage ||
               current.DeepSleepTime != next.DeepSleepTime;
    }

    String serializeConfig(const Configuration *config, bool revealSecrets)
    {
        Serial.println("Serialize config...");
        JsonDocument doc;
        doc["IsConfigured"] = config->IsConfigured;
        doc["ServerAddress"] = config->ServerAddress;
        doc["SensorID"] = config->SensorID;
        doc["WiFiName"] = config->WiFiName;
        // Omitted rather than blanked when redacting. Deserialization now
        // treats absent as "keep" and "" as "clear", so echoing a blank back
        // would wipe the stored secret on the next round trip -- which is
        // precisely what a client doing GET-then-PUT does.
        if (revealSecrets)
        {
            doc["WiFiPassword"] = config->WiFiPassword;
            doc["ApiToken"] = config->ApiToken;
        }
        doc["HasWiFiPassword"] = config->WiFiPassword.length() > 0;
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
        if (!doc.is<JsonObject>())
        {
            Serial.printf("Not a JSON object: %s\n", configStr);
            if (error != nullptr)
            {
                *error = String("Body must be a JSON object");
            }
            return result;
        }

        // Merge, not replace. A key that is absent keeps whatever is already
        // stored, which is what lets a client write
        // {"SunriseSettings":{"IsActivated":true}} without carrying -- and
        // risking clobbering -- the other thirty-odd fields it does not model.
        //
        // This also removes a whole class of per-field default bugs that the
        // old shape had: absent fields used to fall back to hardcoded literals
        // that had drifted from the ones in Configuration's constructor.
        Configuration &parsed = result.second;
        parsed = existingConfig;
        const JsonVariantConst root = doc.as<JsonVariantConst>();

        parsed.IsConfigured = pickBool(root, "IsConfigured", parsed.IsConfigured, mode, error);
        parsed.ServerAddress = pickString(root, "ServerAddress", parsed.ServerAddress, mode, error);
        parsed.SensorID = pickString(root, "SensorID", parsed.SensorID, mode, error);
        parsed.WiFiName = pickString(root, "WiFiName", parsed.WiFiName, mode, error);

        // Absent keeps the stored secret; an explicit "" clears it. The old
        // rule was "blank means keep", which needed a ClearApiToken escape
        // hatch because otherwise a token could never be removed short of
        // reflashing. Serialization omits these keys when redacting, so a
        // GET-then-PUT round trip cannot clear them by accident.
        parsed.WiFiPassword = pickString(root, "WiFiPassword", parsed.WiFiPassword, mode, error);
        parsed.ApiToken = pickString(root, "ApiToken", parsed.ApiToken, mode, error);

        parsed.DhtPin = pickInt(root, "DhtPin", parsed.DhtPin, mode, error);
        parsed.SerialRX = pickInt(root, "SerialRX", parsed.SerialRX, mode, error);
        parsed.SerialTX = pickInt(root, "SerialTX", parsed.SerialTX, mode, error);
        parsed.AnalogSensorPin0 = pickInt(root, "AnalogSensorPin0", parsed.AnalogSensorPin0, mode, error);
        parsed.AnalogSensorPin1 = pickInt(root, "AnalogSensorPin1", parsed.AnalogSensorPin1, mode, error);
        parsed.WindSensorPin = pickInt(root, "WindSensorPin", parsed.WindSensorPin, mode, error);
        parsed.RainfallSensorPin = pickInt(root, "RainfallSensorPin", parsed.RainfallSensorPin, mode, error);
        parsed.LEDPin = pickInt(root, "LEDPin", parsed.LEDPin, mode, error);
        parsed.OneWirePin = pickInt(root, "OneWirePin", parsed.OneWirePin, mode, error);
        parsed.Button1 = pickInt(root, "Button1", parsed.Button1, mode, error);
        parsed.Button2 = pickInt(root, "Button2", parsed.Button2, mode, error);
        parsed.Button2GetURL = pickString(root, "Button2GetURL", parsed.Button2GetURL, mode, error);
        parsed.NumberOfLEDs = pickInt(root, "NumberOfLEDs", parsed.NumberOfLEDs, mode, error);
        parsed.FindSensors = pickBool(root, "FindSensors", parsed.FindSensors, mode, error);
        parsed.IsOfflineMode = pickBool(root, "IsOfflineMode", parsed.IsOfflineMode, mode, error);
        parsed.ShowWebpage = pickBool(root, "ShowWebpage", parsed.ShowWebpage, mode, error);
        parsed.UseMQTT = pickBool(root, "UseMQTT", parsed.UseMQTT, mode, error);
        parsed.MQTTTopic = pickString(root, "MQTTTopic", parsed.MQTTTopic, mode, error);
        parsed.MQTTPort = pickInt(root, "MQTTPort", parsed.MQTTPort, mode, error);
        parsed.DeepSleepTime = pickInt(root, "DeepSleepTime", parsed.DeepSleepTime, mode, error);
        parsed.BufferedValues = pickInt(root, "BufferedValues", parsed.BufferedValues, mode, error);
        parsed.MeasureInterval = pickInt(root, "MeasureInterval", parsed.MeasureInterval, mode, error);

        parsed.LightLow = pickLight(root, "LightLow", parsed.LightLow, mode, error);
        parsed.LightMedium = pickLight(root, "LightMedium", parsed.LightMedium, mode, error);
        parsed.LightHigh = pickLight(root, "LightHigh", parsed.LightHigh, mode, error);

        const JsonVariantConst sunriseSettings = root["SunriseSettings"];
        String sunriseError;
        if (!sunriseSettings.isNull())
        {
            parsed.AlarmSettings = deserializeSunrise(
                sunriseSettings, existingConfig.AlarmSettings, mode, &sunriseError);
        }

        // In strict mode a bad alarm time refuses the document, so the client
        // is told instead of the wrong time being stored. In lenient mode the
        // day kept its previously stored time and the document still loads --
        // refusing it here would make config.cpp replace the whole stored
        // configuration, Wi-Fi credentials included, with defaults.
        if (mode == ParseMode::Strict && !sunriseError.isEmpty())
        {
            Serial.printf("Rejecting config: %s\n", sunriseError.c_str());
            if (error != nullptr && error->isEmpty())
            {
                *error = sunriseError;
            }
            return result;
        }

        // A wrong-typed field recorded by the pick helpers refuses the document
        // in strict mode too, for the same reason: the client should be told
        // rather than have a silently coerced value stored.
        if (mode == ParseMode::Strict && error != nullptr && !error->isEmpty())
        {
            Serial.printf("Rejecting config: %s\n", error->c_str());
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
        SunriseSettings result = existing;
        result.SunriseLightTime = pickDouble(doc, "SunriseLightTime", result.SunriseLightTime, mode, error);
        result.IsActivated = pickBool(doc, "IsActivated", result.IsActivated, mode, error);

        for (int weekdayNumber = Monday; weekdayNumber <= Sunday; ++weekdayNumber)
        {
            const weekday_t weekday = static_cast<weekday_t>(weekdayNumber);
            const JsonVariantConst day = doc[weekdayName(weekday)];
            if (day.isNull())
            {
                // Absent day keeps its stored schedule, so arming the alarm
                // does not need the client to resend all seven days.
                continue;
            }
            if (!day.is<JsonObjectConst>())
            {
                refuse(mode, error, String(weekdayName(weekday)) + ": expected an object");
                continue;
            }
            const auto storedDay = existing.DaySettings.find(weekday);
            const AlarmWeekday previous = storedDay == existing.DaySettings.end()
                                              ? AlarmWeekday()
                                              : storedDay->second;
            String dayError;
            result.DaySettings[weekday] =
                deserializeDaySetting(day, previous, mode, &dayError);
            if (!dayError.isEmpty() && error != nullptr && error->isEmpty())
            {
                *error = String(weekdayName(weekday)) + ": " + dayError;
            }
        }

        // sunrise_alarm.cpp indexes this map with .at(), so every weekday must
        // be present even if `existing` arrived from somewhere that skipped
        // one. emplace leaves days that are already there alone.
        for (int weekdayNumber = Monday; weekdayNumber <= Sunday; ++weekdayNumber)
        {
            result.DaySettings.emplace(static_cast<weekday_t>(weekdayNumber), AlarmWeekday());
        }
        return result;
    }

    AlarmWeekday deserializeDaySetting(JsonVariantConst doc,
                                       const AlarmWeekday &existing,
                                       ParseMode mode,
                                       String *error)
    {
        AlarmWeekday daySetting = existing;
        daySetting.IsActive = pickBool(doc, "IsActive", daySetting.IsActive, mode, error);

        const JsonVariantConst alarmTimeValue = doc["AlarmTime"];
        if (alarmTimeValue.isNull())
        {
            // Toggling a day on or off does not require resending its time.
            return daySetting;
        }

        const String alarmTime = alarmTimeValue.as<String>();
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
