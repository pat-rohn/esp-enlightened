

#include "ts_mqtt.h"
#include <ArduinoJson.h>
#include "timeseries.h"
#include <Ticker.h>

using namespace timeseries;
namespace ts_mqtt
{

    MqttClient *mqttClient;

    CTimeseriesMQTT::CTimeseriesMQTT(
        const String &topic,
        const String &server,
        CTimeHelper *timehelper,
        MqttClient *client) : CTimeseries(server, timehelper),
                              m_Topic(topic)
    {
        mqttClient = client;
    }

    void CTimeseriesMQTT::newValue(const String &name, const double &value)
    {
        Serial.printf("MQTT add value: %s\n", name.c_str());
        String topic = m_Topic + name + "/data";
        if (!mqttClient->connected())
        {
            Serial.println("MQTT: Not connected");
            return;
        }
        String val = convertValue(value);
        mqttClient->beginMessage(topic);
        mqttClient->print(val);
        mqttClient->endMessage();
        Serial.printf("%s %s\n", topic.c_str(), val.c_str());
    }

}
