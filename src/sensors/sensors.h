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
        scd40 = 10,
        ds18b20 = 11,
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

    bool sensorsInit(int serialRX, int serialTX, int oneWirePin);
    void findAndInitSensors();
#if USE_ALL_SENSORS
    void findAndInitMHZ19();
#endif
    std::map<String, SensorData> getValues();
    std::array<SensorData, 3> getDHT22();
    std::array<SensorData, 3> getSCD30();
#if USE_ALL_SENSORS
    std::array<SensorData, 3> getEnv();
    std::array<SensorData, 3> getBME280();
    std::array<SensorData, 3> getMHZ19();
#endif
    std::array<SensorData, 3> getSCD40();
    std::array<SensorData, 3> getWaterValues();
    std::array<SensorData, 3> getWindValues();
    std::array<SensorData, 3> getDS18B20Values();
    void initI2CSensor(uint8_t address);
    void initSCD30();
    bool initSCD40();
    const String &getDescription();

    void scd40printUint16Hex(uint16_t value);
    void scd40printSerialNumber(uint16_t serial0, uint16_t serial1, uint16_t serial2);

}