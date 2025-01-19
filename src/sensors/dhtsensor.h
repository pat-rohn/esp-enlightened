
#include <Arduino.h>

#ifndef DHT_SENSOR_H
#define DHT_SENSOR_H
#include <Arduino.h>
#include <DHT.h>

class DHTSensor
{
#define DHTTYPE DHT22 // DHT11, DHT21, DHT22

public:
    DHTSensor(uint8_t sensorPin)
    {
        pinMode(sensorPin, INPUT);
        Serial.printf("DHT: pin: %d\n", sensorPin);
        m_Dht = new DHT(sensorPin, DHTTYPE);
    }
    ~DHTSensor()
    {
        delete m_Dht;
    }

public:
    bool init()
    {
        m_Dht->begin();
        for (int i = 0; i < 5; i++)
        {
            float h = m_Dht->readHumidity(true);
            float t = m_Dht->readTemperature(false, true);
            Serial.print("DHT: Humidity:");
            Serial.println(h);

            Serial.print("DHT:Temperature:");
            Serial.println(t);
            if (!isnan(h) || !isnan(t))
            {
                Serial.println("DHT: Received DHT value");
                return true;
            }
            delay(50);
        }

        Serial.println("DHT: No response received");
        return true;
    }

    std::pair<float, float> read()
    {
        float h = m_Dht->readHumidity(false);
        float t = m_Dht->readTemperature(false, false);
        return std::make_pair(t, h);
    }

private:
    DHT *m_Dht;
};

#endif