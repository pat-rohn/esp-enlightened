

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
    CTimeseriesHttp::CTimeseriesHttp(const String &timeseriesAddress, CTimeHelper *timehelper)
        : CTimeseries(timeseriesAddress, timehelper)
    {
    }

    void CTimeseriesHttp::newValue(const String &name, const double &value)
    {
        if (m_Data.find(name) == m_Data.end())
        {
            m_Data.insert(std::pair<String, CTimeseriesData>(name, CTimeseriesData(name)));
        }
        m_Data.at(name).addValue(value, m_TimeHelper->getTimestamp());
    }

    bool CTimeseriesHttp::sendData()
    {
        JsonDocument doc;
        for (auto const &ts : m_Data)
        {
            JsonObject tsEntry = doc.add<JsonObject>();
            tsEntry["Tag"] = ts.first;
            JsonArray tsValuesTS = tsEntry["Timestamps"].to<JsonArray>();
            JsonArray tsValuesV = tsEntry["Values"].to<JsonArray>();
            for (auto val : ts.second.m_DataSeries)
            {
                tsValuesTS.add(val.Timestamp);
                String strVal = convertValue(val.Value);
                tsValuesV.add(strVal);
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
}
