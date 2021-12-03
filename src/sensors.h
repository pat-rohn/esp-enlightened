
#include <vector>
#include <Arduino.h>
#include <map>
#include <DHT.h>

enum class SensorType
{
    unknown = 0,
    bme280 = 1,
    dht22 = 2,
    cjmcu = 3,
    bmp280 = 4,
    sht30 = 5,
    sgp30 = 6,
    mhz19 = 6,
};

struct SensorData
{
    String name;
    double value;
    bool isValid;
    String unit;
    SensorData() : name("Unknown"), value(-9999), isValid(false), unit("1")
    {
    }
};

class DHTSensor
{
#define DHTTYPE DHT22 //DHT11, DHT21, DHT22

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

bool sensorsInit(uint8_t dhtPin, uint8_t rx, uint8_t tx);
void findAndInitSensors();
void findAndInitMHZ19();
std::map<String, SensorData> getValues();
std::array<SensorData, 3> getBME280();
std::array<SensorData, 3> getCjmcu();
std::array<SensorData, 3> getDHT22();
std::array<SensorData, 3> getEnv();
std::array<SensorData, 3> getMHZ19();
void initI2CSensor(uint8_t address);
