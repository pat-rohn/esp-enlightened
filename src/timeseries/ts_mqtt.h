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
#include <map>

#include <PubSubClient.h>

using namespace timeseries;

namespace tsmqtt
{
    class CTimeseriesMqtt : public CTimeseries
    {

    public:
        CTimeseriesMqtt(String timeseriesAddress, CTimeHelper *timehelper);
        virtual ~CTimeseriesMqtt(){};

        Device init(const DeviceDesc &deviceDesc) override;
        void addValue(const String &name, const double &value) override;
        bool sendData() override;

    private:
        bool postData();
        bool reconnect();

    private:
        WiFiClient m_WifiClient;
        PubSubClient m_MQTTClient;
    };

    void callback(char *topic, byte *payload, unsigned int length);

}
#endif