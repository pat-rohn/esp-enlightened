#include "timehelper.h"
#include <time.h>
#include <stdio.h>
#include <stdlib.h>

WiFiUDP ntpUDP;

CTimeHelper::CTimeHelper() : m_TimeClient(NTPClient(ntpUDP))
{
    // initTime();
    // timeClient.begin();
    // todo: fix this
    Serial.print("CTimeHelper");
}

bool CTimeHelper::isTimeSet()
{
    return m_TimeClient.isTimeSet();
}

bool CTimeHelper::initTime()
{
    m_TimeClient.setUpdateInterval(120 * 1000);
    configTime(0, 0, "pool.ntp.org", "time.nist.gov");
    unsigned long endTime = millis() + 5000;
    Serial.println("Synchronization...");
    m_TimeClient.begin();
    setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 0);
    tzset();
    while (millis() < endTime)
    {
        Serial.print(" .. ");
        if (m_TimeClient.forceUpdate())
        {
            Serial.println("Timesync success");
            unsigned long timeout = millis() + 60000;
            time_t now;
            while (now < 1651000000 || now >= 35435628436)
            {
                Serial.print("Wait for time...");
                Serial.println(now);
                delay(500);
                time(&now);
                if (millis() > timeout)
                {
                    Serial.println("Timeout while waiting for time. ");
                    break;
                }
            }
            auto time = getHoursAndMinutes();
            Serial.printf("Local time is: %ld:%ld\n", time.first, time.second);

            return true;
        }
        delay(600);
    }

    Serial.println("Failed to sync...");
    return m_TimeClient.update();
}

String CTimeHelper::getTimestamp()
{
    m_TimeClient.update();
    time_t now;
    char strftime_buf[64];
    struct tm timeinfo;

    time(&now);
    gmtime_r(&now, &timeinfo);
    unsigned long millisec = millis();
    strftime(strftime_buf, sizeof(strftime_buf), "%Y-%m-%dT%H:%M:%S.", &timeinfo);
    String timestamp = strftime_buf;
    char buf[10];
    sprintf(buf, "%03lu", millisec % 1000);

    timestamp += buf;
    timestamp += "Z";

    //Serial.printf("UTC time is: %s\n", timestamp.c_str());
   
    return timestamp;
}

String CTimeHelper::fillUpZeros(int number)
{
    String numberStr = "";
    if (number < 10)
    {
        numberStr += 0;
    }
    numberStr += number;
    return numberStr;
}

std::pair<long, long> CTimeHelper::getHoursAndMinutes()
{
    time_t now;
    struct tm timeinfo;
    time(&now);

    localtime_r(&now, &timeinfo);
    // Serial.printf("Time: %d:%d", timeinfo.tm_hour + 1, timeinfo.tm_min);
    return std::pair<int, int>(timeinfo.tm_hour, timeinfo.tm_min); // todo: timezone
}

int CTimeHelper::getWeekDay()
{
    time_t now;
    struct tm timeinfo;

    time(&now);

    localtime_r(&now, &timeinfo);
    switch (timeinfo.tm_wday)
    {
    case 0: // Sunday
        // Serial.printf("Weekday: %d", timeinfo.tm_wday);
        return 6;
        break;
    default:
        // Serial.printf("Weekday: %d", timeinfo.tm_wday);
        return timeinfo.tm_wday - 1;
        break;
    }

    return 0;
}