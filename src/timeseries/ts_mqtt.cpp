

#include "ts_mqtt.h"
#include <ArduinoJson.h>
#include "timeseries.h"
#include <Ticker.h>

using namespace timeseries;
namespace ts_mqtt
{

    CTimeseriesMQTT::CTimeseriesMQTT(
        const String &topic,
        const String &server,
        CTimeHelper *timehelper,
        MqttClient *client) : CTimeseries(server, timehelper),
                              m_Topic(topic),
                              m_Client(client)
    {
    }

    void CTimeseriesMQTT::newValue(const String &name, const double &value)
    {
        Serial.printf("MQTT add value: %s\n", name.c_str());
        String topic = m_Topic + name + "/data";
        if (!m_Client->connected())
        {
            Serial.println("MQTT: Not connected");
            return;
        }
        String val = convertValue(value);
        m_Client->beginMessage(topic);
        m_Client->print(val);
        m_Client->endMessage();
        Serial.printf("%s %s\n", topic.c_str(), val.c_str());
    }

}
