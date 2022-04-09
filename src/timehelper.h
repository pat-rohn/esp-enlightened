#pragma once
#include <Arduino.h>

#include <NTPClient.h>
#include <WiFiUdp.h>

class CTimeHelper
{
public:
    struct Sensor
    {
        String Name;
        double Offset;
        Sensor()
        {
        }
        Sensor(const String &name, double offset)
        {
            this->Name = name;
            this->Offset = Offset;
        }
    };
    struct Device
    {
        String Name;
        std::vector<Sensor> Sensors;
        double Interval;
        int Buffer;

        Device(const String &name, double interval, int buffer)
        {
            this->Name = name;
            this->Interval = interval;
            this->Buffer = buffer;
        }
    };

public:
    CTimeHelper();
    bool initTime();
    String getTimestamp();

    String fillUpZeros(int number);

private:
    NTPClient m_TimeClient;
    bool m_IsTimeInitialized;
};