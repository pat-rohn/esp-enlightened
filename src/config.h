

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
        int WindSensorPin;
        int RainfallSensorPin;
        int LEDPin;
        bool ShowWebpage;
        SunriseSettings AlarmSettings;

        Configuration() : IsConfigured(false),
                          ServerAddress("192.168.1.200:3004"),
                          WiFiName("WiFiName"),
                          WiFiPassword("WifiPW"),
                          FindSensors(true),
                          IsOfflineMode(false),
                          SensorID("Test1"),
                          NumberOfLEDs(-1),
                          DhtPin(-1),
                          WindSensorPin(-1),
                          RainfallSensorPin(-1),
                          LEDPin(-1),
                          ShowWebpage(true),
                          AlarmSettings()
        {
        }
    };

    void begin();
    Configuration readConfig();
    String readConfigAsString();
    bool saveConfig(const Configuration *config);
    void writeConfig(const char *configStr);

    String readFileLFS(const char *path);
    String readFile(fs::FS &fs, const char *path);

    bool writeFile(fs::FS &fs, const char *path, const char *message);
    bool writeFileLFS(const char *path, const char *message);

    std::pair<bool, Configuration> deserializeConfig(const char *configStr);
    SunriseSettings deserializeSunrise(const DynamicJsonDocument &doc);
    AlarmWeekday deserializeDaySetting(const DynamicJsonDocument &doc);

    String serializeConfig(const Configuration *config);
    DynamicJsonDocument serializeSunrise(const SunriseSettings *config);
    DynamicJsonDocument serializeDaySettings(const AlarmWeekday *config);

}

#endif