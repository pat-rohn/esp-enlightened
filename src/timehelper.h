#ifndef TIMEHELPER_H
#define TIMEHELPER_H

#include <Arduino.h>

#include <NTPClient.h>
#include <WiFiUdp.h>

class CTimeHelper
{

public:
    CTimeHelper();
    virtual ~CTimeHelper(){};
    bool isTimeSet();
    bool initTime();
    String getTimestamp();

    String fillUpZeros(int number);

    std::pair<long, long> getHoursAndMinutes();
    int getWeekDay();

private:
    NTPClient m_TimeClient;
};

#endif