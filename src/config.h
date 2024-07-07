

#ifndef CONFIG_H
#define CONFIG_H

#include "LittleFS.h"
#ifdef ESP32
#else
#include <Hash.h>
#include <FS.h>
#endif
#include <map>
#include <ArduinoJson.h>

namespace configman
{
    // static const char kPathToConfig = "config.json";
    const char kPathToConfig[] = "/config.json";
    struct Time
    {
        int Hours;
        int Minutes;
        Time()
        {
            Hours = 8;
            Minutes = 30;
        }

        Time(int hours, int minutes)
        {
            Hours = hours;
            Minutes = minutes;
        }
    };

    struct Light
    {
        int Red;
        int Green;
        int Blue;
        Light()
        {
            Red = 128;
            Green = 128;
            Blue = 128;
        }
        Light(int red, int green, int blue)
        {
            Red = red;
            Green = green;
            Blue = blue;
        }
    };

    enum weekday_t
    {
        Monday,
        Tuesday,
        Wednesday,
        Thursday,
        Friday,
        Saturday,
        Sunday
    };

    struct AlarmWeekday
    {
        bool IsActive;
        Time AlarmTime;
        AlarmWeekday() : IsActive(false),
                         AlarmTime(Time()){};
    };

    struct SunriseSettings
    {
        bool IsActivated;
        double SunriseLightTime;
        std::map<weekday_t, AlarmWeekday> DaySettings;
        SunriseSettings() : IsActivated(false),
                            SunriseLightTime(20.0)
        {
            DaySettings = std::map<weekday_t, AlarmWeekday>{};
            for (int weekDay = weekday_t::Monday; weekDay <= weekday_t::Sunday; weekDay++)
            {
                DaySettings[static_cast<weekday_t>(weekDay)] = AlarmWeekday();
            }
        }
        SunriseSettings(const SunriseSettings * settings);
    };

    struct Configuration
    {
        bool IsConfigured;
        String ServerAddress;
        String WiFiName;
        String WiFiPassword;
        bool FindSensors;
        bool IsOfflineMode;
        String SensorID;
        int NumberOfLEDs;
        int DhtPin;
        int SerialRX;
        int SerialTX;
        int WindSensorPin;
        int RainfallSensorPin;
        int LEDPin;
        int Button1;
        int Button2;
        String Button2GetURL;
        bool ShowWebpage;
        bool UseMQTT;
        String MQTTTopic;
        int MQTTPort;
        SunriseSettings AlarmSettings;
        Light LightLow;
        Light LightMedium;
        Light LightHigh;

        Configuration();
        Configuration(const Configuration *c);
    };

    void begin();

    Configuration getConfig();
    void setConfig(Configuration config);

    Configuration readConfig();
    String readConfigAsString();
    bool saveConfig(const Configuration *config);
    void writeConfig(const char *configStr);

    String readFileLFS(const char *path);
    String readFile(fs::FS &fs, const char *path);

    bool writeFile(fs::FS &fs, const char *path, const char *message);
    bool writeFileLFS(const char *path, const char *message);

    std::pair<bool, Configuration> deserializeConfig(const char *configStr);
    SunriseSettings deserializeSunrise(const JsonDocument &doc);
    AlarmWeekday deserializeDaySetting(const JsonDocument &doc);

    String serializeConfig(const Configuration *config);
    JsonDocument serializeSunrise(const SunriseSettings *config);
    JsonDocument serializeDaySettings(const AlarmWeekday *config);

}

#endif