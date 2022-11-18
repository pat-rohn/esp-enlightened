

#include "ts_http.h"
#include <ArduinoJson.h>
#include "timeseries.h"

#ifdef ESP8266
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#endif /* ESP8266 */

#ifdef ESP32
#include "WiFi.h"
#include <HTTPClient.h>
#endif /* ESP32 */

using namespace timeseries;

namespace ts_http
{
    CTimeseriesHttp::CTimeseriesHttp(String timeseriesAddress, CTimeHelper *timehelper)
        : CTimeseries(timeseriesAddress, timehelper)
    {
    }

    Device CTimeseriesHttp::init(const DeviceDesc &deviceDesc)
    {
        Serial.println("HTTP: INIT");
        return CTimeseries::init(deviceDesc);
    };

    void CTimeseriesHttp::addValue(const String &name, const double &value)
    {
        if (m_Data.find(name) == m_Data.end())
        {
            m_Data.insert(std::pair<String, CTimeseriesData>(name, CTimeseriesData(name)));
        }
        m_Data.at(name).addValue(value, m_TimeHelper->getTimestamp());
    }

    bool CTimeseriesHttp::sendData()
    {
        // https://arduinojson.org/v6/assistant/
        // https://arduinojson.org/v6/how-to/determine-the-capacity-of-the-jsondocument/

        DynamicJsonDocument doc(10000); // uses heap because it's too much data for stack
        for (auto const &ts : m_Data)
        {
            JsonObject tsEntry = doc.createNestedObject();
            tsEntry["Tag"] = ts.first;
            JsonArray tsValuesTS = tsEntry.createNestedArray("Timestamps");
            JsonArray tsValuesV = tsEntry.createNestedArray("Values");
            for (auto val : ts.second.m_DataSeries)
            {
                tsValuesTS.add(val.Timestamp);
                if (val.Value < 0.00001)
                {
                    tsValuesV.add(String(val.Value, 8));
                }
                else if (val.Value < 0.001)
                {
                    tsValuesV.add(String(val.Value, 5));
                }
                else
                {
                    tsValuesV.add(String(val.Value, 4));
                }
            }
        }
        Serial.println(doc.as<String>());
        if (postData(doc.as<String>(), "/timeseries/save"))
        {
            m_Data.clear();
            return true;
        }
        return false;
    }

    bool CTimeseriesHttp::postData(const String &root, const String &url)
    {
        WiFiClient client = WiFiClient();
        HTTPClient http;
        String serverPath = "http://" + m_ServerAddress + url;
        Serial.println(serverPath);

        http.begin(client, serverPath.c_str());
        Serial.println("Post data");
        int httpResponseCode = http.POST(root.c_str());

        if (httpResponseCode > 0)
        {
            Serial.print("HTTP Response code: ");
            Serial.println(httpResponseCode);
            String payload = http.getString();
            Serial.println(payload);
            http.end();
            return true;
        }
        else
        {
            Serial.print("Error code: ");
            Serial.println(httpResponseCode);
            http.end();
            return false;
        }
        return true;
    }
}