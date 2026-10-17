

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

        // ServerAddress may already carry its scheme; only a bare host:port gets http://.
        m_ServerAddress = timeseriesAddress.indexOf("://") >= 0 ? timeseriesAddress
                                                                : "http://" + timeseriesAddress;
    };
    

    bool CTimeseries::initDevice(const DeviceDesc &device)
    {
        JsonDocument doc;
        JsonObject devObj = doc["Device"].to<JsonObject>();
        devObj["Name"] = device.Name;
        devObj["Description"] = device.Description;
        JsonArray sensors = devObj["Sensors"].to<JsonArray>();
        for (const auto &s : device.Sensors)
        {
            sensors.add(s);
        }
        Serial.print("initDevice: ");
        Serial.println(doc.as<String>());
        return postData(doc.as<String>(), "/init-device");
    }

    bool CTimeseries::postData(const String &root, const String &url)
    {
        WiFiClient client = WiFiClient();
        HTTPClient http;
        String serverPath = m_ServerAddress + url;
        Serial.println(serverPath);

        http.begin(client, serverPath.c_str());
        Serial.println("Post data");
        int httpResponseCode = http.POST(root.c_str());

        if (httpResponseCode >= 200 && httpResponseCode < 300)
        {
            Serial.print("HTTP Response code: ");
            Serial.println(httpResponseCode);
            String payload = http.getString();
            Serial.println(payload);
            http.end();
            return true;
        }
        Serial.print("Error code: ");
        Serial.println(httpResponseCode);
        http.end();
        return false;
    }

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

    const String brokerHost(const String &serverAddress)
    {
        int start = serverAddress.indexOf("://");
        start = start >= 0 ? start + 3 : 0;
        int end = serverAddress.length();
        for (int i = start; i < end; i++)
        {
            char c = serverAddress.charAt(i);
            if (c == ':' || c == '/')
            {
                end = i;
                break;
            }
        }
        return serverAddress.substring(start, end);
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
