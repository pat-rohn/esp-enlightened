
#include <vector>
#include <Arduino.h>
#include <map>

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

bool sensorsInit();
std::vector<SensorType> findAndInitSensors();
std::map<String, SensorData> getValues();
std::array<SensorData, 3> getBME280();
std::array<SensorData, 3>  getCjmcu();
std::array<SensorData, 3>  getDHT22(bool force = false);
std::array<SensorData, 3>  getEnv();
void initSensor(uint8_t address);
