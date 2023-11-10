

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
                                                           WindSensorPin(c->WindSensorPin),
                                                           RainfallSensorPin(c->RainfallSensorPin),
                                                           LEDPin(c->LEDPin),
                                                           Button1(c->Button1),
                                                           Button2(c->Button2),
                                                           ShowWebpage(c->ShowWebpage),
                                                           UseMQTT(c->UseMQTT),
                                                           MQTTTopic(c->MQTTTopic),
                                                           MQTTPort(c->MQTTPort),
                                                           AlarmSettings(SunriseSettings(c->AlarmSettings))
    {
    }
    Configuration::Configuration() : WiFiPassword("WifiPW"),
                                     FindSensors(true),
                                     IsOfflineMode(false),
                                     SensorID("Test1"),
                                     NumberOfLEDs(-1),
                                     DhtPin(-1),
                                     WindSensorPin(-1),
                                     RainfallSensorPin(-1),
                                     LEDPin(-1),
                                     Button1(-1),
                                     Button2(-1),
                                     ShowWebpage(true),
                                     UseMQTT(false),
                                     MQTTTopic(""),
                                     MQTTPort(1883),
                                     AlarmSettings()
    {
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
        DynamicJsonDocument doc(4096);
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
        auto config = serializeConfig(&c);
        if (!writeFileLFS(kPathToConfig, config.c_str()))
        {
            Serial.print("Failed to write config.");
        }
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
        DynamicJsonDocument doc(8000);
        doc["IsConfigured"] = config->IsConfigured;
        doc["ServerAddress"] = config->ServerAddress;
        doc["SensorID"] = config->SensorID;
        doc["WiFiName"] = config->WiFiName;
        doc["WiFiPassword"] = config->WiFiPassword;
        doc["DhtPin"] = config->DhtPin;
        doc["WindSensorPin"] = config->WindSensorPin;
        doc["RainfallSensorPin"] = config->RainfallSensorPin;
        doc["LEDPin"] = config->LEDPin;
        doc["Button1"] = config->Button1;
        doc["Button2"] = config->Button2;
        doc["NumberOfLEDs"] = config->NumberOfLEDs;
        doc["FindSensors"] = config->FindSensors;
        doc["IsOfflineMode"] = config->IsOfflineMode;
        doc["ShowWebpage"] = config->ShowWebpage;
        doc["UseMQTT"] = config->UseMQTT;
        doc["MQTTTopic"] = config->MQTTTopic;
        doc["MQTTPort"] = config->MQTTPort;

        doc["SunriseSettings"] = serializeSunrise(&config->AlarmSettings);

        char buffer[2000];
        serializeJsonPretty(doc, buffer);

        return String(buffer);
    }

    DynamicJsonDocument serializeSunrise(const SunriseSettings *config)
    {
        DynamicJsonDocument doc(450);
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

    DynamicJsonDocument serializeDaySettings(const AlarmWeekday *config)
    {
        DynamicJsonDocument doc(60);
        doc["AlarmTime"] = std::to_string(config->AlarmTime.Hours) + ":" + std::to_string(config->AlarmTime.Minutes);
        doc["IsActive"] = config->IsActive;

        return doc;
    }

    std::pair<bool, Configuration> deserializeConfig(const char *configStr)
    {
        std::pair<bool, Configuration> res = std::pair<bool, Configuration>(false, Configuration());
        DynamicJsonDocument doc(4096);
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
        res.second.WindSensorPin = doc["WindSensorPin"];
        res.second.RainfallSensorPin = doc["RainfallSensorPin"];
        res.second.LEDPin = doc["LEDPin"];
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

        res.first = true;
        return res;
    }

    SunriseSettings deserializeSunrise(const DynamicJsonDocument &doc)
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

    AlarmWeekday deserializeDaySetting(const DynamicJsonDocument &doc)
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
