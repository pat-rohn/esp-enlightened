
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
        Serial.print("Pin Nr:");
        Serial.println(sensorPin);
        m_Dht = new DHT(sensorPin, DHTTYPE);
    }
    ~DHTSensor()
    {
        m_Dht = nullptr;
    }

public:
    bool init()
    {
        m_Dht->begin();
        for (int i = 0; i < 5; i++)
        {
            float h = m_Dht->readHumidity(true);
            float t = m_Dht->readTemperature(false, true);
            Serial.print("Humidity:");
            Serial.println(h);

            Serial.print("Temperature:");
            Serial.println(t);
            if (!isnan(h) || !isnan(t))
            {
                Serial.println("Has DHT Sensor");
                return true;
            }
            delay(250);
        }

        Serial.println("No DHT Sensor");
        return false;
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