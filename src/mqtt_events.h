
#ifndef MQTT_EVENTS_H
#define MQTT_EVENTS_H

#include <Arduino.h>
#include "ArduinoMqttClient.h"
#include <array>

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
/*
Light JSON Topic used for Home Assistant:
  light:
    - name: "JSON light"
      schema: json
      state_topic: "mylamp/json/status"
      command_topic: "mylamp/json/set"
      brightness: true
      brightness_scale: 100
      color_mode: true
      supported_color_modes: ["rgb"]
*/

    void setup(MqttClient *mqttClient, const String topic);
    void subscribe();
    bool poll();
    void onMqttMessage(int messageSize);
    void sendStateTopic(std::array<uint8_t, 3> colors, bool on, double factor);
    std::array<uint8_t,3> getRGB();
    double getBrightness();
    bool getIsOn();
}

#endif