#ifndef TIMERSERIES_H
#define TIMERSERIES_H

#include <map>
#include <array>
#include <vector>
#include <Arduino.h>
#include "timehelper.h"

namespace timeseries
{

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

        void addValue(const double &value, String timestamp)
        {
            if (!timestamp.isEmpty())
            {
                m_DataSeries.emplace_back(DataPoint(timestamp, value));
            }
        }

        std::vector<DataPoint> m_DataSeries;
        String m_Name;
    };

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

    class CTimeseries
    {

    public:
        CTimeseries(const String &timeseriesAddress, CTimeHelper *timehelper);
        virtual ~CTimeseries(){};

    public:
        virtual Device init(const DeviceDesc &deviceDesc);
        Device deserializeDevice(const String &deviceJson);
        virtual void newValue(const String &name, const double &value) = 0;
        virtual bool sendData() { return true; };

    protected:
        String splitAddress(String serverAddressWithPort, int index);
        String convertValue(double value);

    protected:
        String m_ServerAddress;
        std::map<String, CTimeseriesData> m_Data;
        CTimeHelper *m_TimeHelper;
    };
}

#endif