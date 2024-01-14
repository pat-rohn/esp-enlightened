
#ifndef MQTT_EVENTS_H
#define MQTT_EVENTS_H

#include <Arduino.h>
#include "ArduinoMqttClient.h"

#ifdef ESP8266
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#endif /* ESP8266 */

#ifdef ESP32
#include "WiFi.h"
#include <HTTPClient.h>
#endif /* ESP32 */

namespace mqtt_events
{
    void setup(MqttClient *mqttClient, const String topic);
    bool poll();
    void onMqttMessage(int messageSize);
}

#endif