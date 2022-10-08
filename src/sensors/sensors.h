
#include <vector>
#include <Arduino.h>
#include <map>
#include "dhtsensor.h"

namespace sensor
{

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
        scd30 = 7,
        watersensor = 8,
        windsensor = 9,
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

    bool sensorsInit();
    void findAndInitSensors();
    void findAndInitMHZ19();
    std::map<String, SensorData> getValues();
    std::array<SensorData, 3> getBME280();
    std::array<SensorData, 3> getCjmcu();
    std::array<SensorData, 3> getDHT22();
    std::array<SensorData, 3> getEnv();
    std::array<SensorData, 3> getMHZ19();
    std::array<SensorData, 3> getSCD30();
    std::array<SensorData, 3> getWaterValues();
    std::array<SensorData, 3> getWindValues();
    void initI2CSensor(uint8_t address);
    void initSCD30();
    std::vector<String> getValueNames();
    const String &getDescription();

}