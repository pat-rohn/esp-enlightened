

#include "ts_mqtt.h"
#include <ArduinoJson.h>
#include "timeseries.h"
#include <Ticker.h>

using namespace timeseries;
namespace tsmqtt
{

    CTimeseriesMqtt::CTimeseriesMqtt(String timeseriesAddress, CTimeHelper *timehelper)
        : CTimeseries(timeseriesAddress, timehelper)

    {

        IPAddress ip;
        ip.fromString(timeseriesAddress.c_str());
        m_WifiClient = WiFiClient();
        m_MQTTClient = PubSubClient(ip, 1883, callback, m_WifiClient);
        m_ServerAddress += timeseriesAddress;
    }

    Device CTimeseriesMqtt::init(const DeviceDesc &deviceDesc)
    {
        return CTimeseries::init(deviceDesc);
    };

    void CTimeseriesMqtt::addValue(const String &name, const double &value)
    {
        if (m_Data.find(name) == m_Data.end())
        {
            m_Data.insert(std::pair<String, CTimeseriesData>(name, CTimeseriesData(name)));
        }
        m_Data.at(name).addValue(value, m_TimeHelper->getTimestamp());
    }

    bool CTimeseriesMqtt::sendData()
    {
        if (!WiFi.isConnected())
        {
            Serial.println("No WiFi");
            // return false;
        }

        for (auto const &ts : m_Data)
        {
            DynamicJsonDocument doc(10000); // uses heap because it's too much data for stack
            JsonObject tsEntry = doc.createNestedObject();
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

            // TODO: Publish
        }
        m_Data.clear();
        return true;
    }

    bool CTimeseriesMqtt::reconnect()
    {
        Serial.println("Connecting to MQTT...");

        if (!m_MQTTClient.connected())
        {
            Serial.println("Connecting to MQTT Broker...");
            while (!m_MQTTClient.connected())
            {
                Serial.println("Reconnecting to MQTT Broker..");
                String clientId = "ESP8266Client-";
                clientId += String(random(0xffff), HEX);

                if (m_MQTTClient.connect(clientId.c_str()))
                {
                    Serial.println("Connected.");
                    // subscribe to topic
                    return true;
                }
            }
        }
        return false;
    }

    void callback(char *topic, byte *payload, unsigned int length)
    {
        Serial.println("Callback");
        Serial.println((char)payload[0]);
    }
}
