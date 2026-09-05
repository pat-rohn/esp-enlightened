#ifndef DOMAIN_CONFIGURATION_H
#define DOMAIN_CONFIGURATION_H

#include <Arduino.h>
#include <map>

namespace configuration
{
    struct Time
    {
        int Hours;
        int Minutes;
        Time() : Hours(8), Minutes(30) {}
        Time(int hours, int minutes) : Hours(hours), Minutes(minutes) {}
    };

    struct Light
    {
        int Red;
        int Green;
        int Blue;
        Light() : Red(128), Green(128), Blue(128) {}
        Light(int red, int green, int blue) : Red(red), Green(green), Blue(blue) {}
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
        AlarmWeekday() : IsActive(false), AlarmTime() {}
    };

    struct SunriseSettings
    {
        bool IsActivated;
        double SunriseLightTime;
        std::map<weekday_t, AlarmWeekday> DaySettings;

        SunriseSettings() : IsActivated(false), SunriseLightTime(20.0)
        {
            for (int weekDay = Monday; weekDay <= Sunday; weekDay++)
            {
                DaySettings[static_cast<weekday_t>(weekDay)] = AlarmWeekday();
            }
        }

        SunriseSettings(const SunriseSettings *settings) : SunriseSettings(*settings) {}
    };

    struct Configuration
    {
        bool IsConfigured;
        String ServerAddress;
        String WiFiName;
        String WiFiPassword;
        String ApiToken;
        bool FindSensors;
        bool IsOfflineMode;
        String SensorID;
        int NumberOfLEDs;
        int DhtPin;
        int SerialRX;
        int SerialTX;
        int AnalogSensorPin0;
        int AnalogSensorPin1;
        int WindSensorPin;
        int RainfallSensorPin;
        int LEDPin;
        int OneWirePin;
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
        int DeepSleepTime;
        int BufferedValues;
        int MeasureInterval;

        Configuration() : IsConfigured(false),
                          ServerAddress(""),
                          WiFiName("Enlighted"),
                          WiFiPassword("enlighten-me"),
                          ApiToken(""),
                          FindSensors(false),
                          IsOfflineMode(true),
                          SensorID("Test1"),
                          NumberOfLEDs(-1),
                          DhtPin(-1),
                          SerialRX(-1),
                          SerialTX(-1),
                          AnalogSensorPin0(-1),
                          AnalogSensorPin1(-1),
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
                          LightLow(16, 4, 0),
                          LightMedium(64, 32, 4),
                          LightHigh(128, 64, 24),
                          DeepSleepTime(-1),
                          BufferedValues(3),
                          MeasureInterval(30)
        {
        }

        Configuration(const Configuration *configuration) : Configuration(*configuration) {}
    };
}

#endif
