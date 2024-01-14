#include "mqtt_events.h"

namespace mqtt_events
{
    MqttClient *mqttClient;
    bool wasStriggered = false;
    String trigger_topic = "test/lamp";

    void setup(MqttClient *client, const String topic)
    {
        mqttClient = client;
        trigger_topic = topic+"trigger";

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
        Serial.println(trigger_topic);
        Serial.println();

        mqttClient->subscribe(trigger_topic.c_str());
        // topics can be unsubscribed using:
        // mqttClient->unsubscribe(topic);
        Serial.print("Topic: ");
        Serial.println(topic);
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
        Serial.print(mqttClient->messageTopic());
        Serial.print("', length ");
        Serial.print(messageSize);
        Serial.println(" bytes:");
        // use the Stream interface to print the contents
        while (mqttClient->available())
        {
            Serial.print((char)mqttClient->read());
        }
        Serial.println();
    }

}