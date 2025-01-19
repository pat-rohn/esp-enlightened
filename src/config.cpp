

#include "config.h"

namespace configman
{
    Configuration config = Configuration();

    SunriseSettings::SunriseSettings(const SunriseSettings *s) : IsActivated(s->IsActivated),
                                                                 SunriseLightTime(s->SunriseLightTime)
    {
        DaySettings = std::map<weekday_t, AlarmWeekday>{};
        for (int weekDay = weekday_t::Monday; weekDay <= weekday_t::Sunday; weekDay++)
        {
            DaySettings[static_cast<weekday_t>(weekDay)] = AlarmWeekday();
            DaySettings[static_cast<weekday_t>(weekDay)].AlarmTime = s->DaySettings.at(static_cast<weekday_t>(weekDay)).AlarmTime;
            DaySettings[static_cast<weekday_t>(weekDay)].IsActive = s->DaySettings.at(static_cast<weekday_t>(weekDay)).IsActive;
        }
    }

    Configuration::Configuration(const Configuration *c) : IsConfigured(c->IsConfigured),
                                                           ServerAddress(c->ServerAddress),
                                                           WiFiName(c->WiFiName),
                                                           WiFiPassword(c->WiFiPassword),
                                                           FindSensors(c->FindSensors),
                                                           IsOfflineMode(c->IsOfflineMode),
                                                           SensorID(c->SensorID),
                                                           NumberOfLEDs(c->NumberOfLEDs),
                                                           DhtPin(c->DhtPin),
                                                           SerialRX(c->SerialRX),
                                                           SerialTX(c->SerialTX),
                                                           WindSensorPin(c->WindSensorPin),
                                                           RainfallSensorPin(c->RainfallSensorPin),
                                                           LEDPin(c->LEDPin),
                                                           OneWirePin(c->OneWirePin),
                                                           Button1(c->Button1),
                                                           Button2(c->Button2),
                                                           ShowWebpage(c->ShowWebpage),
                                                           UseMQTT(c->UseMQTT),
                                                           MQTTTopic(c->MQTTTopic),
                                                           MQTTPort(c->MQTTPort),
                                                           AlarmSettings(SunriseSettings(c->AlarmSettings)),
                                                           DeepSleepTime(c->DeepSleepTime),
                                                           BufferedValues(c->BufferedValues),
                                                           MeasureInterval(c->MeasureInterval)
    {
    }
    Configuration::Configuration() : IsConfigured(false),
                                     ServerAddress(""),
                                     WiFiName("Enlighted"),
                                     WiFiPassword("enlighten-me"),
                                     FindSensors(false),
                                     IsOfflineMode(true),
                                     SensorID("Test1"),
                                     NumberOfLEDs(-1),
                                     DhtPin(-1),
                                     SerialRX(-1),
                                     SerialTX(-1),
                                     WindSensorPin(-1),
                                     RainfallSensorPin(-1),
                                     LEDPin(-1),
                                     OneWirePin(-1),
                                     Button1(-1),
                                     Button2(-1),
                                     Button2GetURL("http://192.168.1.125/relay/0?turn=toggle"),
                                     ShowWebpage(true),
                                     UseMQTT(false),
                                     MQTTTopic(""),
                                     MQTTPort(1883),
                                     AlarmSettings(),
                                     DeepSleepTime(-1),
                                     BufferedValues(3),
                                     MeasureInterval(30)
    {
        LightLow = Light(16, 4, 0);
        LightMedium = Light(64, 32, 4);
        LightHigh = Light(128, 64, 24);
    }

    void begin()
    {
        Serial.println("LittleFS.begin()");
#ifdef ESP8266
        if (!LittleFS.begin())
        {
            Serial.println("Failed to mount LittleFS");
        }
        else
        {
            Serial.println("LittleFS succesfully mounted");
        }
#endif
#ifdef ESP32
        if (!LittleFS.begin(false))
        {
            Serial.println("Failed to mount LittleFS");
            if (!LittleFS.begin(true))
            {
                Serial.println("Failed to format LittleFS");
            }
            else
            {
                Serial.println("LittleFS formatted successfully");
            }
        }
        else
        {
            Serial.println("LittleFS succesfully mounted");
        }
#endif
    }

    Configuration getConfig()
    {
        return config;
    }
    void setConfig(Configuration config)
    {
        config = config;
    }

    Configuration readConfig()
    {
        String configStr = readFileLFS(kPathToConfig);
        if (configStr.length() <= 0)
        {
            Serial.println("No config was stored");
            auto defaultConf = Configuration();
            if (!saveConfig(&defaultConf))
            {
                Serial.println("Failed to read or create config...");
                return Configuration();
            }
            delay(500);
            Serial.println("Retry reading config...");
            return readConfig();
        }

        Serial.printf("Stored config: \n%s\n", configStr.c_str());
        auto res = deserializeConfig(configStr.c_str());
        if (!res.first)
        {
            Serial.println("Well, store default then");
            auto defaultConf = Configuration();
            if (!saveConfig(&defaultConf))
            {
                Serial.println("Failed to save config, return default for now...");
                return Configuration();
            }
        }
        config = res.second;
        return getConfig();
    }

    String readConfigAsString()
    {
        Serial.println("readConfigAsString");
        auto configStr = readFileLFS(kPathToConfig);
        auto res = deserializeConfig(configStr.c_str());
        if (configStr.isEmpty() || !res.first)
        {
            Configuration defaultConf = Configuration();
            Serial.print("Invalid config. write default.");
            if (!saveConfig(&defaultConf))
            {
                Serial.print("Failed to write default.");
                return "{}";
            }
            delay(1000);
            return readConfigAsString();
        }
        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, configStr.c_str());
        if (err.code() != DeserializationError::Code::Ok)
        {
            return String(err.code());
        }

        char buffer[2000];

        serializeJsonPretty(doc, buffer);

        return String(buffer);
    }

    bool saveConfig(const Configuration *c)
    {
        config = Configuration(c);
        String confStr = serializeConfig(c);
        return writeFileLFS(kPathToConfig, confStr.c_str());
    }

    void writeConfig(const char *configStr)
    {
        Serial.println("Write config.");
        auto res = deserializeConfig(configStr);
        if (!res.first)
        {
            Serial.print("Invalid config.");
            return;
        }
        Configuration c = res.second;
        auto cStr = serializeConfig(&c);
        if (!writeFileLFS(kPathToConfig, cStr.c_str()))
        {
            Serial.print("Failed to write config.");
            return;
        }
        config = c;
    }

    String readFile(fs::FS &fs, const char *path)
    {
        Serial.printf("Reading file: %s\r\n", path);
        File file = fs.open(path, "r");
        if (!file || file.isDirectory())
        {
            Serial.println("- empty file or failed to open file");
            return String();
        }
        String fileContent;
        while (file.available())
        {
            fileContent += String((char)file.read());
        }
        // Serial.println(fileContent);
        return fileContent;
    }

    String readFileLFS(const char *path)
    {
        return readFile(LittleFS, path);
    }

    bool writeFile(fs::FS &fs, const char *path, const char *message)
    {
        Serial.printf("Writing file: %s\r\n", path);
        File file = fs.open(path, "w");
        if (!file)
        {
            Serial.println("- failed to open file for writing");
            return false;
        }
        if (file.print(message))
        {
            Serial.println("- file written");
        }
        else
        {
            Serial.println("- write failed");
            return false;
        }
        return true;
    }

    bool writeFileLFS(const char *path, const char *message)
    {
        return writeFile(LittleFS, path, message);
    }

    String serializeConfig(const Configuration *config)
    {
        JsonDocument doc;
        doc["IsConfigured"] = config->IsConfigured;
        doc["ServerAddress"] = config->ServerAddress;
        doc["SensorID"] = config->SensorID;
        doc["WiFiName"] = config->WiFiName;
        doc["WiFiPassword"] = config->WiFiPassword;
        doc["DhtPin"] = config->DhtPin;
        doc["SerialRX"] = config->SerialRX;
        doc["SerialTX"] = config->SerialTX;
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

        char buffer[2000];
        serializeJsonPretty(doc, buffer);

        return String(buffer);
    }

    JsonDocument serializeSunrise(const SunriseSettings *config)
    {
        JsonDocument doc;
        for (int weekDayN = weekday_t::Monday; weekDayN <= weekday_t::Sunday; weekDayN++)
        {
            doc["IsActivated"] = config->IsActivated;
            doc["SunriseLightTime"] = config->SunriseLightTime;
            weekday_t weekDay = static_cast<weekday_t>(weekDayN);
            switch (weekDay)
            {
            case weekday_t::Monday:
                doc["Monday"] = serializeDaySettings(&config->DaySettings.at(weekday_t::Monday));
                break;
            case weekday_t::Tuesday:
                doc["Tuesday"] = serializeDaySettings(&config->DaySettings.at(weekday_t::Tuesday));
                break;
            case weekday_t::Wednesday:
                doc["Wednesday"] = serializeDaySettings(&config->DaySettings.at(weekday_t::Wednesday));
                break;
            case weekday_t::Thursday:
                doc["Thursday"] = serializeDaySettings(&config->DaySettings.at(weekday_t::Thursday));
                break;
            case weekday_t::Friday:
                doc["Friday"] = serializeDaySettings(&config->DaySettings.at(weekday_t::Friday));
                break;
            case weekday_t::Saturday:
                doc["Saturday"] = serializeDaySettings(&config->DaySettings.at(weekday_t::Saturday));
                break;
            case weekday_t::Sunday:
                doc["Sunday"] = serializeDaySettings(&config->DaySettings.at(weekday_t::Sunday));
                break;

            default:
                break;
            }
        }

        return doc;
    }

    JsonDocument serializeDaySettings(const AlarmWeekday *config)
    {
        JsonDocument doc;
        doc["AlarmTime"] = std::to_string(config->AlarmTime.Hours) + ":" + std::to_string(config->AlarmTime.Minutes);
        doc["IsActive"] = config->IsActive;

        return doc;
    }

    std::pair<bool, Configuration> deserializeConfig(const char *configStr)
    {
        std::pair<bool, Configuration> res = std::pair<bool, Configuration>(false, Configuration());
        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, configStr);
        if (err.code() != DeserializationError::Code::Ok)
        {
            Serial.printf("Deserializing failed %d\n", err.code());
            Serial.print(configStr);
            return res;
        }
        if (!doc.containsKey("IsConfigured"))
        {
            Serial.printf("No valid config %s\n", configStr);
            return res;
        }
        res.second.IsConfigured = doc["IsConfigured"];
        res.second.ServerAddress = doc["ServerAddress"].as<String>();
        res.second.SensorID = doc["SensorID"].as<String>();
        res.second.WiFiName = doc["WiFiName"].as<String>();
        res.second.WiFiPassword = doc["WiFiPassword"].as<String>();
        res.second.DhtPin = doc["DhtPin"];
        JsonVariant serialRX = doc["SerialRX"];
        if (serialRX.isNull())
        {
            Serial.println("Serial Pins not configured");
            res.second.SerialRX = -1;
            res.second.SerialTX = -1;
        }
        else
        {
            res.second.SerialRX = doc["SerialRX"];
            res.second.SerialTX = doc["SerialTX"];
        }
        res.second.WindSensorPin = doc["WindSensorPin"];
        res.second.RainfallSensorPin = doc["RainfallSensorPin"];
        res.second.LEDPin = doc["LEDPin"];
        JsonVariant oneWire = doc["OneWirePin"];
        if (oneWire.isNull())
        {
            Serial.println("One wire does not exist (yet?)");
            res.second.OneWirePin = -1;
            res.second.DeepSleepTime = -1;
            res.second.BufferedValues = 3;
            res.second.MeasureInterval = 30;
        }
        else
        {
            res.second.OneWirePin = doc["OneWirePin"];
            res.second.DeepSleepTime = doc["DeepSleepTime"];
            res.second.BufferedValues = doc["BufferedValues"];
            res.second.MeasureInterval = doc["MeasureInterval"];
        }
        JsonVariant button1 = doc["Button1"];
        if (button1.isNull())
        {
            Serial.println("Button configs do not exist (yet?)");
            res.second.Button1 = -1;
            res.second.Button2 = -1;
        }
        else
        {
            res.second.Button1 = doc["Button1"];
            res.second.Button2 = doc["Button2"];
        }

        JsonVariant button2GetURL = doc["Button2GetURL"];
        if (button2GetURL.isNull())
        {
            res.second.Button2GetURL = "http://192.168.1.125/relay/0?turn=toggle";
            Serial.println("Button 2 get URL set to " + res.second.Button2GetURL);
        }
        else
        {
            res.second.Button2GetURL = doc["Button2GetURL"].as<String>();
        }

        res.second.NumberOfLEDs = doc["NumberOfLEDs"];
        res.second.FindSensors = doc["FindSensors"];
        res.second.IsOfflineMode = doc["IsOfflineMode"];
        res.second.ShowWebpage = doc["ShowWebpage"];

        JsonVariant UseMQTT = doc["UseMQTT"];
        if (UseMQTT.isNull())
        {
            Serial.println("UseMQTT did not exist");
            res.second.UseMQTT = false;
            res.second.MQTTPort = 1883;
            res.second.MQTTTopic = "/myplace/myroom/";
        }
        else
        {
            res.second.UseMQTT = doc["UseMQTT"];
            res.second.MQTTPort = doc["MQTTPort"];
            res.second.MQTTTopic = doc["MQTTTopic"].as<String>();
        }

        JsonVariant sunriseSettings = doc["SunriseSettings"];
        if (!sunriseSettings.isNull())
        {
            auto resSunrise = deserializeSunrise(sunriseSettings);
            res.second.AlarmSettings = resSunrise;
        }
        else
        {
            Serial.println("Warning: Alarm clock does not exist");
            res.second.AlarmSettings = SunriseSettings();
            if (saveConfig(&res.second))
            {
                String configStr = readFileLFS(kPathToConfig);
                ::delay(100);
                return deserializeConfig(configStr.c_str());
            }
        }
        JsonVariant lightLow = doc["LightLow"];
        if (lightLow.isNull())
        {
            Serial.println("Light settings do not exist (yet?)");
            res.second.LightLow = Light(32, 4, 0);
            res.second.LightMedium = Light(64, 32, 4);
            res.second.LightHigh = Light(128, 64, 24);
        }
        else
        {
            res.second.LightLow.Red = lightLow["Red"];
            res.second.LightLow.Green = lightLow["Green"];
            res.second.LightLow.Blue = lightLow["Blue"];

            JsonVariant LightMedium = doc["LightMedium"];

            res.second.LightMedium.Red = LightMedium["Red"];
            res.second.LightMedium.Green = LightMedium["Green"];
            res.second.LightMedium.Blue = LightMedium["Blue"];

            JsonVariant LightHigh = doc["LightHigh"];

            res.second.LightHigh.Red = LightHigh["Red"];
            res.second.LightHigh.Green = LightHigh["Green"];
            res.second.LightHigh.Blue = LightHigh["Blue"];
        }
        res.first = true;
        return res;
    }

    SunriseSettings deserializeSunrise(const JsonDocument &doc)
    {
        SunriseSettings res = SunriseSettings();

        res.SunriseLightTime = doc["SunriseLightTime"];
        res.IsActivated = doc["IsActivated"];
        if (res.IsActivated)
        {
            Serial.print("\n Sunrise is activated ");
        }
        for (int weekDayN = weekday_t::Monday; weekDayN <= weekday_t::Sunday; weekDayN++)
        {
            weekday_t weekDay = static_cast<weekday_t>(weekDayN);

            switch (weekDay)
            {
            case weekday_t::Monday:
                if (!doc.containsKey("Monday"))
                {
                    res.DaySettings[weekday_t::Monday] = configman::AlarmWeekday();
                }
                res.DaySettings[weekday_t::Monday] = deserializeDaySetting(doc["Monday"]);
                break;
            case weekday_t::Tuesday:
                if (!doc.containsKey("Tuesday"))
                {
                    res.DaySettings[weekday_t::Monday] = configman::AlarmWeekday();
                }
                res.DaySettings[weekday_t::Tuesday] = deserializeDaySetting(doc["Tuesday"]);
                break;
            case weekday_t::Wednesday:
                if (!doc.containsKey("Wednesday"))
                {
                    res.DaySettings[weekday_t::Monday] = configman::AlarmWeekday();
                }
                res.DaySettings[weekday_t::Wednesday] = deserializeDaySetting(doc["Wednesday"]);
                break;
            case weekday_t::Thursday:
                if (!doc.containsKey("Thursday"))
                {
                    res.DaySettings[weekday_t::Monday] = configman::AlarmWeekday();
                }
                res.DaySettings[weekday_t::Thursday] = deserializeDaySetting(doc["Thursday"]);
                break;
            case weekday_t::Friday:
                if (!doc.containsKey("Friday"))
                {
                    res.DaySettings[weekday_t::Monday] = configman::AlarmWeekday();
                }
                res.DaySettings[weekday_t::Friday] = deserializeDaySetting(doc["Friday"]);
                break;
            case weekday_t::Saturday:
                if (!doc.containsKey("Saturday"))
                {
                    res.DaySettings[weekday_t::Monday] = configman::AlarmWeekday();
                }
                res.DaySettings[weekday_t::Saturday] = deserializeDaySetting(doc["Saturday"]);
                break;
            case weekday_t::Sunday:
                if (!doc.containsKey("Sunday"))
                {
                    res.DaySettings[weekday_t::Monday] = configman::AlarmWeekday();
                }
                res.DaySettings[weekday_t::Sunday] = deserializeDaySetting(doc["Sunday"]);
                break;

            default:
                break;
            }
        }

        return res;
    }

    AlarmWeekday deserializeDaySetting(const JsonDocument &doc)
    {
        AlarmWeekday daySetting = AlarmWeekday();

        daySetting.IsActive = doc["IsActive"];

        String alarmTime = doc["AlarmTime"].as<String>();
        int index = alarmTime.lastIndexOf(':');
        int length = alarmTime.length();
        if (length < 2)
        {
            Serial.printf("Invalid alarm time %s\n", alarmTime.c_str());
            daySetting.AlarmTime = configman::Time();
        }
        else
        {
            String minutes = alarmTime.substring(index + 1, length);
            String hours = alarmTime.substring(0, index);

            daySetting.AlarmTime = Time(hours.toInt(), minutes.toInt());
        }

        return daySetting;
    }
}
