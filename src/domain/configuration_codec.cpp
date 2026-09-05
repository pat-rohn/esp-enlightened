#include "configuration_codec.h"

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
        doc["AlarmTime"] = std::to_string(config->AlarmTime.Hours) + ":" +
                           std::to_string(config->AlarmTime.Minutes);
        doc["IsActive"] = config->IsActive;
        return doc;
    }

    std::pair<bool, Configuration> deserializeConfig(
        const char *configStr, const Configuration &existingConfig)
    {
        std::pair<bool, Configuration> result(false, Configuration());
        JsonDocument doc;
        const DeserializationError err = deserializeJson(doc, configStr);
        if (err.code() != DeserializationError::Code::Ok)
        {
            Serial.printf("Deserializing failed %d\n", err.code());
            Serial.print(configStr);
            return result;
        }
        if (!doc["IsConfigured"].is<bool>())
        {
            Serial.printf("No valid config %s\n", configStr);
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
        const String incomingApiToken = doc["ApiToken"].as<String>();
        parsed.ApiToken = incomingApiToken.length() > 0
                              ? incomingApiToken
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
        if (!sunriseSettings.isNull())
        {
            parsed.AlarmSettings = deserializeSunrise(sunriseSettings);
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

        result.first = true;
        Serial.println("Success: Deserialized config");
        return result;
    }

    SunriseSettings deserializeSunrise(JsonVariantConst doc)
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
            result.DaySettings[weekday] = day.is<JsonObjectConst>()
                                              ? deserializeDaySetting(day)
                                              : AlarmWeekday();
        }
        return result;
    }

    AlarmWeekday deserializeDaySetting(JsonVariantConst doc)
    {
        AlarmWeekday daySetting;
        daySetting.IsActive = doc["IsActive"];

        const String alarmTime = doc["AlarmTime"].as<String>();
        const int index = alarmTime.lastIndexOf(':');
        const int length = alarmTime.length();
        if (length < 2)
        {
            Serial.printf("Invalid alarm time %s\n", alarmTime.c_str());
            daySetting.AlarmTime = Time();
        }
        else
        {
            const String minutes = alarmTime.substring(index + 1, length);
            const String hours = alarmTime.substring(0, index);
            daySetting.AlarmTime = Time(hours.toInt(), minutes.toInt());
        }
        return daySetting;
    }
}
