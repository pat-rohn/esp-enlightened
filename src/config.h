

#ifndef CONFIG_H
#define CONFIG_H

#include "LittleFS.h"
#ifdef ESP32
#else
#include <Hash.h>
#include <FS.h>
#endif

namespace configman
{
    // static const char kPathToConfig = "config.json";
    const char kPathToConfig[] = "config.json";
    struct Time
    {
        int Hours;
        int Minutes;
        Time(){
            Hours = 0;
            Minutes = 0;
        }

        Time(int hours, int minutes){
            Hours = hours;
            Minutes = minutes;
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
        bool IsSunriseAlarm;
        Time AlarmTime;

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
                          IsSunriseAlarm(false),
                          AlarmTime(Time())
        {
        }
    };

    void begin();
    Configuration readConfig();

    bool saveConfig(const Configuration *config);

    void writeConfig(const char *configStr);

    String readFile(fs::FS &fs, const char *path);
    String readFileLFS(const char *path);
    bool writeFile(fs::FS &fs, const char *path, const char *message);
    bool writeFileLFS(const char *path, const char *message);

    String serializeConfig(const Configuration *config);
    std::pair<bool, Configuration> deserializeConfig(const char *configStr);
    String readConfigAsString();

}

#endif