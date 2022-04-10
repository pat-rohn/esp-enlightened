#pragma once
#include <Arduino.h>

#include <NTPClient.h>
#include <WiFiUdp.h>

class CTimeHelper
{

public:
    CTimeHelper();
    bool initTime();
    String getTimestamp();

    String fillUpZeros(int number);

private:
    NTPClient m_TimeClient;
    bool m_IsTimeInitialized;
};