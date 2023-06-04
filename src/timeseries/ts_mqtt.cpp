

#include "ts_mqtt.h"
#include <ArduinoJson.h>
#include "timeseries.h"
#include <Ticker.h>

using namespace timeseries;
namespace ts_mqtt
{

    WiFiClient wifiClient;
    MqttClient *mqttClient;

    CTimeseriesMQTT::CTimeseriesMQTT(
        MQTTProperties &properties,
        CTimeHelper *timehelper) : CTimeseries(properties.Host, timehelper),
                                   m_Topic(properties.Topic)
    {
        m_Host = splitAddress(properties.Host, 0);
        Serial.printf("MQTT address: %s\n", m_Host.c_str());
        mqttClient = new MqttClient(wifiClient);
    }

    void CTimeseriesMQTT::newValue(const String &name, const double &value)
    {
        Serial.printf("MQTT add value: %s\n", name.c_str());
        String topic = m_Topic + name + "/data";
        if (!mqttClient->connected())
        {
            if (!mqttClient->connect(m_Host.c_str(), 1883))
            {
                Serial.print("MQTT connection failed! Error code = ");
                Serial.println(mqttClient->connectError());
            }
        }
        String val = convertValue(value);
        mqttClient->beginMessage(topic);
        mqttClient->print(val);
        mqttClient->endMessage();
        Serial.printf("%s %s\n", topic.c_str(), val.c_str());
    }

}
