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
    CTimeseries(const char *timeseriesAddress, const char * port);

public:
    void addValue(const String &name, const double &value);
    bool sendData();

private:
    bool postData(const String &root, const String &url);

private:
    String m_ServerAddress;
    std::map<String, CTimeseriesData> m_Data;

    CTimeHelper m_TimeHelper;
};
