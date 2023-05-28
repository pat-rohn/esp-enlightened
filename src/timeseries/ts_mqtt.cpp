

#include "ts_mqtt.h"
#include <ArduinoJson.h>
#include "timeseries.h"
#include <Ticker.h>

using namespace timeseries;
namespace ts_mqtt
{

    WiFiClient wifiClient;
    MqttClient *mqttClient;

    CTimeseriesMQTT::CTimeseriesMQTT(const String &timeseriesAddress, CTimeHelper *timehelper) : CTimeseries(timeseriesAddress, timehelper)
    {
        m_Host = splitAddress(timeseriesAddress, 0);
        Serial.printf("MQTT address: %s\n", m_Host.c_str());
        mqttClient = new MqttClient(wifiClient);
    }

    void CTimeseriesMQTT::newValue(const String &name, const double &value)
    {
        Serial.printf("MQTT add value: %s\n", name.c_str());
        if (!mqttClient->connected())
        {
            if (!mqttClient->connect(m_Host.c_str(), 1883))
            {
                Serial.print("MQTT connection failed! Error code = ");
                Serial.println(mqttClient->connectError());
            }
        }
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
        mqttClient->beginMessage(name);
        mqttClient->print(value);
        mqttClient->endMessage();
        Serial.printf("%s %s\n", name.c_str(), timeseriesValue.c_str());
    }


}
