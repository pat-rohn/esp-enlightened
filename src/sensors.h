
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
        m_Dht->readHumidity(false);
        float t = m_Dht->readTemperature(false, false);
        return !isnan(t);
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

bool sensorsInit(uint8_t dhtPin);
void findAndInitSensors();
std::map<String, SensorData> getValues();
std::array<SensorData, 3> getBME280();
std::array<SensorData, 3> getCjmcu();
std::array<SensorData, 3> getDHT22();
std::array<SensorData, 3> getEnv();
void initI2CSensor(uint8_t address);
