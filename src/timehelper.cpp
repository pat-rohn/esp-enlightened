#include "timehelper.h"

WiFiUDP ntpUDP;

CTimeHelper::CTimeHelper() : m_TimeClient(NTPClient(ntpUDP)), m_IsTimeInitialized(false)
{
    //initTime();
    //timeClient.begin();
    // todo: fix this
    Serial.print("CTimeHelper");
}

bool CTimeHelper::initTime()
{
    m_TimeClient.setUpdateInterval(120 * 1000);
    configTime(0, 0, "pool.ntp.org", "time.nist.gov");
    unsigned long endTime = millis() + 10000;
    Serial.println("Synchronization...");
    m_TimeClient.begin();
    while (millis() < endTime)
    {
        Serial.print(" .. ");
        if (m_TimeClient.forceUpdate())
        {
            Serial.print("Success");
            return true;
        }
        delay(600);
    }

    Serial.println("");
    return m_TimeClient.update();
}

String CTimeHelper::getTimestamp()
{
    if (!m_IsTimeInitialized)
    {
        m_IsTimeInitialized = initTime();
        return getTimestamp();
    }
    time_t now;
    char strftime_buf[64];
    struct tm timeinfo;

    time(&now);

    localtime_r(&now, &timeinfo);
    unsigned long millisec = millis();
    strftime(strftime_buf, sizeof(strftime_buf), "%Y-%m-%dT%H:%M:%S.", &timeinfo);
    String timestamp = strftime_buf;
    char buf[10];
    sprintf(buf, "%03lu", millisec % 1000);

    timestamp += buf;
    timestamp += "Z";

    //Serial.print("Time:");
    //Serial.println(timestamp);
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
