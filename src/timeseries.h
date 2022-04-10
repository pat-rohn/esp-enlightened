#pragma once
#include <map>
#include <array>
#include <vector>
#include <Arduino.h>
#include "timehelper.h"

class CTimeseriesData
{
    struct DataPoint
    {
        String Timestamp;
        double Value;
        DataPoint(String timestamp, const double &value) : Timestamp(timestamp), Value(value)
        {
        }
    };

public:
    CTimeseriesData(const String &name)
    {
        m_Name = name;
    };

    void addValue(const double &value, String timestamp);

    std::vector<DataPoint> m_DataSeries;
    String m_Name;
};

class CTimeseries
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

    struct DeviceDesc
    {
        String Name;
        std::vector<String> Sensors;
        String Description;
        DeviceDesc(const String &name, const String &desc)
        {
            this->Name = name;
            this->Description = desc;
        }
    };

public:
    CTimeseries(const String &timeseriesAddress, const String &port);

public:
    CTimeseries::Device init(const CTimeseries::DeviceDesc &deviceDesc);
    void addValue(const String &name, const double &value);
    bool sendData();

private:
    bool postData(const String &root, const String &url);
    CTimeseries::Device deserializeDevice(const char *deviceJson);

private:
    String m_ServerAddress;
    std::map<String, CTimeseriesData> m_Data;

    CTimeHelper m_TimeHelper;
};
