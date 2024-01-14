#ifndef TIMERSERIES_MQTT_H
#define TIMERSERIES_MQTT_H

#include <Arduino.h>

#ifdef ESP8266
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#endif /* ESP8266 */

#ifdef ESP32
#include "WiFi.h"
#include <HTTPClient.h>
#endif /* ESP32 */

#include "timeseries/timeseries.h"
#include "timehelper.h"
#include "ArduinoMqttClient.h"
#include <map>

using namespace timeseries;

namespace ts_mqtt
{
    class CTimeseriesMQTT : public CTimeseries
    {

    public:
        CTimeseriesMQTT(const String &topic, const String &server,
                        CTimeHelper *timehelper, MqttClient *client);
        virtual ~CTimeseriesMQTT(){};

        void newValue(const String &name, const double &value);

    private:
        String m_Host;
        String m_Topic;
    };

}
#endif