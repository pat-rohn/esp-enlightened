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
#include <set>

using namespace timeseries;

namespace ts_mqtt
{
    class CTimeseriesMQTT : public CTimeseries
    {

    public:
        CTimeseriesMQTT(const String &topic, const String &server,
                        CTimeHelper *timehelper, MqttClient *client,
                        const String &deviceID = "");
        virtual ~CTimeseriesMQTT(){};

        void newValue(const String &name, const double &value);
        bool reportHealth(const DeviceHealth &health) override;

    private:
        bool publishDiscovery(const DeviceHealth &health);
        void publishMeasurementDiscovery(const String &metric);
        String m_DeviceID;
        bool m_DiagnosticsDiscovered = false;
        std::set<String> m_DiscoveredMetrics;
        String m_Topic;
        MqttClient *m_Client;
    };

}
#endif