#include "mqtt_events.h"
#include <ArduinoJson.h>

namespace mqtt_events
{
    MqttClient *mqttClient;
    bool wasStriggered = false;
    String topic = "";
    std::array<uint8_t, 3> colors;
    bool isOn;
    int brightness = 100;

    void setup(MqttClient *client, const String t)
    {
        mqttClient = client;
        topic = t;

        if (!mqttClient->connected())
        {
            Serial.println("Mqtt Event: Not connected.");
            return;
        }
        Serial.println("You're connected to the MQTT broker!");
        Serial.println();
        // set the message receive callback
        mqttClient->onMessage(onMqttMessage);
        Serial.print("Subscribing to topic: ");
        Serial.println();

        mqttClient->subscribe(String(topic + "set").c_str());
        mqttClient->subscribe(String(topic + "json/set").c_str());
        mqttClient->subscribe(String(topic + "switch").c_str());
        // mqttClient->subscribe(String(topic + "status").c_str());
        //  topics can be unsubscribed using:
        //  mqttClient->unsubscribe(topic);
        Serial.print("Topic: ");
        Serial.println(topic);
        colors = {0, 0, 0};
        isOn = false;
    }

    void sendStateTopic(std::array<uint8_t, 3> c, bool on, double factor)
    {
        if (!mqttClient->connected())
        {
            Serial.println("MQTT: Not connected");
            return;
        }
        brightness = factor*100;
        isOn = on;

        StaticJsonDocument<512> doc;
        /*
        {
            "color_mode": "rgb",
            "state": "ON",
            "brightness": 6,
            "color": {
                "r": 100,
                "g": 70,
                "b": 35
            }
        }
        */
        doc["color_mode"] = "rgb";
        doc["state"] = on ? "ON" : "OFF";
        doc["brightness"] = brightness;
        StaticJsonDocument<200> docRGB;
        docRGB["r"] = c[0];
        docRGB["g"] = c[1];
        docRGB["b"] = c[2];
        doc["color"] = docRGB;

        char buffer[512];
        serializeJsonPretty(doc, buffer);

        mqttClient->beginMessage(String(topic + "json/status").c_str());
        mqttClient->print(buffer);
        Serial.printf("%s %s\n", String(topic + "json/status").c_str(), buffer);
        mqttClient->endMessage();

        colors = c;
    }

    bool poll()
    {
        if (mqttClient == nullptr)
        {
            Serial.println("No mqtt Client");
            return false;
        }
        if (!mqttClient->connected())
        {
            return false;
        }
        mqttClient->poll();
        if (wasStriggered)
        {
            wasStriggered = false;
            return true;
        }
        return false;
    }

    void onMqttMessage(int messageSize)
    {
        wasStriggered = true;
        // we received a message, print out the topic and contents
        Serial.println("Received a message with topic '");
        String topic = mqttClient->messageTopic();
        Serial.print(topic);
        Serial.print("', length ");
        Serial.print(messageSize);
        Serial.println(" bytes:");
        // use the Stream interface to print the contents
        String payload = "";
        while (mqttClient->available())
        {
            payload += (char)mqttClient->read();
        }
        Serial.println(payload);
        if (topic.indexOf("/json/set") > 0)
        {
            Serial.println("JSON SET");
            StaticJsonDocument<512> doc;
            deserializeJson(doc, payload);
            // example{"state":"ON","color":{"r":140,"g":249,"b":255}}
            String state = doc["state"];
            if (!doc["brightness"].isNull())
            {
                brightness = doc["brightness"];
                Serial.printf("brightness %d\n", brightness);
            }
            StaticJsonDocument<200> rgb = doc["color"];
            if (!rgb.isNull())
            {
                colors[0] = rgb["r"];
                colors[1] = rgb["g"];
                colors[2] = rgb["b"];
            }
            if (state.indexOf("ON") >= 0)
            {
                Serial.println("Switch On");
                isOn = true;
            }
            else
            {
                Serial.println("Switch Off");
                isOn = false;
            }
        }

        Serial.println();
    }

    std::array<uint8_t, 3> getRGB()
    {
        return colors;
    }

    bool getIsOn()
    {
        return isOn;
    }

    double getBrightness()
    {
        return brightness;
    }
}
