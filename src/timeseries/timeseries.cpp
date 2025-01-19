

#include "timeseries.h"
#include <ArduinoJson.h>

#ifdef ESP8266
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#endif /* ESP8266 */

#ifdef ESP32
#include "WiFi.h"
#include <HTTPClient.h>
#endif /* ESP32 */

namespace timeseries
{
    CTimeseries::CTimeseries(const String &timeseriesAddress, CTimeHelper *timehelper)
    {
        Serial.printf("Timeseries Server: %s\n", timeseriesAddress.c_str());
        m_TimeHelper = timehelper;

        String serverPath = "http://" + timeseriesAddress;
        m_ServerAddress = serverPath;
    };
    

    String CTimeseries::convertValue(double value)
    {
        String timeseriesValue = "";
        if (value < 0.00001)
        {
            timeseriesValue += String(value, 8);
        }
        else if (value < 0.001)
        {
            timeseriesValue += String(value, 5);
        }
        else
        {
            timeseriesValue += String(value, 4);
        }
        return timeseriesValue;
    }

    const String splitAddress(const String & serverAddressWithPort, int index)
    {
        int found = 0;
        int strIndex[] = {0, -1};
        int maxIndex = serverAddressWithPort.length() - 1;
        char separator = ':';

        for (int i = 0; i <= maxIndex && found <= index; i++)
        {
            {
                if (serverAddressWithPort.charAt(i) == separator || i == maxIndex)
                {
                    found++;
                    strIndex[0] = strIndex[1] + 1;
                    strIndex[1] = (i == maxIndex) ? i + 1 : i;
                }
            }
        }
        return found > index ? serverAddressWithPort.substring(strIndex[0], strIndex[1]) : "";
    }

}
