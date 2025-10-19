#include "sensors.h"
// #include <M5Core2.h>
#include <Adafruit_Sensor.h>

#include "DHT.h"
#include <Wire.h>
#include <SPI.h>
#include <set>
// #include "Adafruit_SGP30.h"
// #include <WEMOS_SHT3X.h>
#include "config.h"
#include "sensors/watersensor.h"
#include "sensors/windsensor.h"
#include <SoftwareSerial.h>
#include "one_wire.h"

// unfortunately some ESP32 (e.g. firebeetle esp32-e)

#if DISABLE_SCD40
#include <SensirionI2CScd4x.h>
#endif

#if USE_ALL_SENSORS
#include <MHZ19.h>
#include <Adafruit_BME280.h>
#include <Adafruit_BMP280.h>
#endif
#include <SparkFun_SCD30_Arduino_Library.h>

int waterSensorPin = -1;
int windSensorPin = -1;
int rx = -1;
int tx = -1;

#if DISABLE_SCD40
SensirionI2CScd4x scd4x;
#endif

namespace sensor
{

    std::set<SensorType> m_SensorTypes;
    String m_Description = "";

    TwoWire MyWire = Wire;

    SCD30 airSensor;
#if USE_ALL_SENSORS
    MHZ19 myMHZ19;
    Adafruit_BME280 bme; // I2C
    Adafruit_BMP280 bmp = Adafruit_BMP280(&MyWire);
    Adafruit_Sensor *bmp_temp = bmp.getTemperatureSensor();
    Adafruit_Sensor *bmp_pressure = bmp.getPressureSensor();
    // SHT3X sht30(0x44);
#endif

    DHTSensor *dhtSensor;

    // Adafruit_SGP30 sgp;

    SoftwareSerial MySerial;

    bool sensorsInit(int serialRX, int serialTX, int oneWirePin)
    {
        Serial.println("Sensors init.");
        configman::Configuration config = configman::readConfig();

        m_SensorTypes.clear();
        if (config.DhtPin >= 0)
        {
            if (dhtSensor != nullptr)
            {
                delete dhtSensor;
            }
            dhtSensor = new DHTSensor(config.DhtPin);

            if (dhtSensor->init())
            {
                m_SensorTypes.insert(SensorType::dht22);
                m_Description = m_Description + "DHT22;";
            }
        }
        if (serialRX >= 0 && serialTX >= 0)
        {
            rx = serialRX;
            tx = serialTX;
            Serial.printf("Serial pins set to %d and %d\n", rx, tx);
        }
        else
        {
            Serial.println("No serial pins configured");
        }
        findAndInitSensors();
        if (config.RainfallSensorPin >= 0)
        {
            m_SensorTypes.insert(SensorType::watersensor);
            watersensor::start(config.RainfallSensorPin);
            m_Description = m_Description + "Rain;";
        }
        if (config.WindSensorPin >= 0)
        {
            m_SensorTypes.insert(SensorType::windsensor);
            windsensor::start(config.WindSensorPin);
            m_Description = m_Description + "Wind;";
        }

        if (oneWirePin > 0)
        {
            if (one_wire::init(oneWirePin))
            {
                m_SensorTypes.insert(SensorType::ds18b20);
                m_Description = m_Description + "Temperature;";
            }
        }

        if (m_SensorTypes.empty())
        {
            // MyWire.begin(32, 33);
            Serial.println("Change Wire since nothing found.");
            m_Description = m_Description + "No Sensors;";
            return false;
        }
        return true;
    }

    void findAndInitSensors()
    {
        Serial.println("Find and init i2c sensors");
        byte count = 0;
        MyWire.begin(SDA, SCL);
        bool hasSCD40 = false;
#if DISABLE_SCD40
        hasSCD40 = initSCD40();
#endif
        if (!hasSCD40) // so far only this i2c device alone supported
        {
            for (byte i = 8; i < 120; i++)
            {
                MyWire.beginTransmission(i);
                if (MyWire.endTransmission() == 0)
                {
                    Serial.print("Found address: ");
                    Serial.print(i, DEC);
                    Serial.print(" (0x");
                    Serial.print(i, HEX);
                    Serial.println(")");
                    count++;
                    delay(100);
                    initI2CSensor(i);
                }
                delay(50);
            }
        }

        Serial.println();

#if USE_ALL_SENSORS
        findAndInitMHZ19();
#endif
    }

#if USE_ALL_SENSORS
    void findAndInitMHZ19()
    {
        Serial.println("Find and init MHZ19 sensor");
        if (rx < 0 || tx < 0)
        {
            Serial.printf("rx and tx pins not configured %d, %d\n ", rx, tx);
            return;
        }
        MySerial.begin(9600, SWSERIAL_8N1, rx, tx);
        myMHZ19.begin(MySerial);
        myMHZ19.verify();
        delay(500);

        char version[4];
        myMHZ19.getVersion(version);
        Serial.println(version);
        Serial.print("\nFirmware Version: ");
        String v = "";
        for (byte i = 0; i < 4; i++)
        {
            v += version[i];

            if (i == 1)
            {
                v += ".";
            }
        }
        Serial.println(v);
        if (v == "04.43" || v == "05.02") // Todo: Find generic way
        {
            Serial.println("Found sensor");
            m_SensorTypes.insert(SensorType::mhz19);
            m_Description = m_Description + "MHZ19(" + v + ");";
            myMHZ19.autoCalibration();
            return;
        }
    }
#endif

    std::map<String, SensorData> getValues()
    {
        if (m_SensorTypes.empty())
        {
            Serial.println("No Sensors detected?");
        }
        std::map<String, SensorData> res;

        if (m_SensorTypes.find(SensorType::dht22) != m_SensorTypes.end())
        {
            for (const auto &val : getDHT22())
            {
                if (val.isValid)
                {
                    res[val.name] = val;
                }
            }
        }
        if (m_SensorTypes.find(SensorType::scd30) != m_SensorTypes.end())
        {
            for (const auto &val : getSCD30())
            {
                if (val.isValid)
                {
                    res[val.name] = val;
                }
            }
        }
#if USE_ALL_SENSORS
        if (m_SensorTypes.find(SensorType::bme280) != m_SensorTypes.end())
        {
            for (const auto &val : getBME280())
            {
                if (val.isValid)
                {
                    res[val.name] = val;
                }
            }
        }
        if (m_SensorTypes.find(SensorType::bmp280) != m_SensorTypes.end())
        {
            for (const auto &val : getEnv())
            {
                if (val.isValid)
                {
                    res[val.name] = val;
                }
            }
        }
        if (m_SensorTypes.find(SensorType::mhz19) != m_SensorTypes.end())
        {
            for (const auto &val : getMHZ19())
            {
                if (val.isValid)
                {
                    res[val.name] = val;
                }
            }
        }

        if (m_SensorTypes.find(SensorType::scd40) != m_SensorTypes.end())
        {
            for (const auto &val : getSCD40())
            {
                if (val.isValid)
                {
                    res[val.name] = val;
                }
            }
        }
#endif
        if (m_SensorTypes.find(SensorType::watersensor) != m_SensorTypes.end())
        {
            for (const auto &val : getWaterValues())
            {
                if (val.isValid)
                {
                    res[val.name] = val;
                }
            }
        }
        if (m_SensorTypes.find(SensorType::windsensor) != m_SensorTypes.end())
        {
            for (const auto &val : getWindValues())
            {
                if (val.isValid)
                {
                    res[val.name] = val;
                }
            }
        }
        if (m_SensorTypes.find(SensorType::ds18b20) != m_SensorTypes.end())
        {
            for (const auto &val : getDS18B20Values())
            {
                if (val.isValid)
                {
                    res[val.name] = val;
                }
            }
        }
        return res;
    }

    std::array<SensorData, 3> getDHT22()
    {
        std::array<SensorData, 3> sensorData;
        sensorData.fill(SensorData());
        std::pair<float, float> values = dhtSensor->read();

        if (!isnan(values.first))
        {
            sensorData[0].isValid = true;
            sensorData[0].value = values.first;
            sensorData[0].unit = "*C";
            sensorData[0].name = "Temperature";
            Serial.print(sensorData[0].name);
            Serial.print(": ");
            Serial.print(sensorData[0].value);
            Serial.println(sensorData[0].unit);
        }
        else
        {
            Serial.print("DHT no valid result:");
            Serial.println(values.first);
        }
        if (!isnan(values.second))
        {
            sensorData[1].isValid = true;
            sensorData[1].value = values.second;
            sensorData[1].unit = "%";
            sensorData[1].name = "Humidity";

            Serial.print(sensorData[1].name);
            Serial.print(": ");
            Serial.print(sensorData[1].value);
            Serial.println(sensorData[1].unit);
        }
        return sensorData;
    }

#if USE_ALL_SENSORS
    std::array<SensorData, 3> getBME280()
    {
        std::array<SensorData, 3> sensorData;
        float temperature = bme.readTemperature();
        float pressure = bme.readPressure();
        float humidity = bme.readHumidity();

        if (isnan(temperature) || isnan(pressure) || isnan(humidity))
        {
            Serial.println("Couldn't read value from BME280");
            return sensorData;
        }

        if (pressure < 100)
        {
            Serial.println("No BME Connection");
            return sensorData;
        }
        sensorData[0].isValid = true;
        sensorData[0].value = temperature;
        sensorData[0].unit = "*C";
        sensorData[0].name = "Temperature";

        sensorData[1].isValid = true;
        sensorData[1].value = pressure;
        sensorData[1].unit = "mbar";
        sensorData[1].name = "Pressure";

        sensorData[2].isValid = true;
        sensorData[2].value = humidity;
        sensorData[2].unit = "%";
        sensorData[2].name = "Humidity";

        return sensorData;
    }

    std::array<SensorData, 3> getEnv()
    {
        std::array<SensorData, 3> sensorData;
        sensorData.fill(SensorData());
        if (!bmp.begin(0x76))
        {
            Serial.println(F("Could not find a valid BMP280 sensor, check wiring or "
                             "try a different address!"));
            return sensorData;
        }
        sensors_event_t temp_event, pressure_event;
        bmp_temp->getEvent(&temp_event);
        bmp_pressure->getEvent(&pressure_event);
        sensorData[0].isValid = true;
        sensorData[0].value = temp_event.temperature;
        sensorData[0].unit = "*C";
        sensorData[0].name = "Temperature";
        sensorData[1].isValid = true;
        sensorData[1].value = pressure_event.pressure;
        sensorData[1].unit = "mbar";
        sensorData[1].name = "Pressure";

        unsigned int data[6];

        // Start I2C Transmission
        MyWire.beginTransmission(0x44);
        // Send measurement command
        MyWire.write(0x2C);
        MyWire.write(0x06);
        // Stop I2C transmission
        if (MyWire.endTransmission() != 0)
        {
            return sensorData;
        }

        delay(500);

        // Request 6 bytes of data
        MyWire.requestFrom(0x44, 6);

        // Read 6 bytes of data
        // cTemp msb, cTemp lsb, cTemp crc, humidity msb, humidity lsb, humidity crc
        for (int i = 0; i < 6; i++)
        {
            data[i] = MyWire.read();
        };

        delay(50);

        if (MyWire.available() != 0)
        {
            return sensorData;
        }

        // Convert the data
        // double cTemp = ((((data[0] * 256.0) + data[1]) * 175) / 65535.0) - 45;
        // fTemp = (cTemp * 1.8) + 32;
        double humidity = ((((data[3] * 256.0) + data[4]) * 100) / 65535.0);
        sensorData[2].isValid = true;
        sensorData[2].value = humidity;
        sensorData[2].unit = "%";
        sensorData[2].name = "Humidity";

        return sensorData;
    }

    std::array<SensorData, 3> getMHZ19()
    {
        std::array<SensorData, 3> sensorData;
        sensorData.fill(SensorData());
        int CO2 = myMHZ19.getCO2();

        Serial.print("CO2 (ppm): ");
        Serial.println(CO2);

        if (CO2 > 0)
        {
            sensorData[0].isValid = true;
            sensorData[0].value = CO2;
            sensorData[0].unit = "ppm";
            sensorData[0].name = "CO2";
        }

        return sensorData;
    }
#endif

    std::array<SensorData, 3> getSCD40()
    {
        std::array<SensorData, 3> sensorData;
        Serial.print("Fetch SCD40 ");
#if DISABLE_SCD40
        uint16_t error;
        char errorMessage[256];
        uint16_t co2 = 0;
        float temperature = 0.0f;
        float humidity = 0.0f;
        bool isDataReady = false;
        error = scd4x.getDataReadyFlag(isDataReady);
        if (error)
        {
            Serial.print("Error trying to execute getDataReadyFlag(): ");
            errorToString(error, errorMessage, 256);
            Serial.println(errorMessage);
            return sensorData;
        }
        if (!isDataReady)
        {
            return sensorData;
        }
        error = scd4x.readMeasurement(co2, temperature, humidity);
        if (error)
        {
            Serial.print("Error trying to execute readMeasurement(): ");
            errorToString(error, errorMessage, 256);
            Serial.println(errorMessage);
            return sensorData;
        }
        else if (co2 == 0)
        {
            Serial.println("Invalid sample detected, skipping.");
            return sensorData;
        }
        if (co2 > 0)
        {
            sensorData[0].isValid = true;
            sensorData[0].value = co2;
            sensorData[0].unit = "ppm";
            sensorData[0].name = "CO2";
            if (co2 < 400)
            {
                // todo: calibrate
            }
        }
        if (temperature > 0)
        {
            sensorData[1].isValid = true;
            sensorData[1].value = temperature;
            sensorData[1].unit = "*C";
            sensorData[1].name = "Temperature";
        }
        if (humidity > 0)
        {
            sensorData[2].isValid = true;
            sensorData[2].value = humidity;
            sensorData[2].unit = "%";
            sensorData[2].name = "Humidity";
        }
#endif

        return sensorData;
    }

    std::array<SensorData, 3> getSCD30()
    {
        std::array<SensorData, 3> sensorData;
        if (!airSensor.dataAvailable())
        {
            Serial.println("SCD30: Data not available.");
            return sensorData;
        }
        sensorData.fill(SensorData());
        float CO2 = airSensor.getCO2();
        float temperature = airSensor.getTemperature();
        float humidity = airSensor.getHumidity();

        Serial.print("CO2 (ppm): ");
        Serial.println(CO2);
        Serial.print("Temperature (°C): ");
        Serial.println(temperature);
        Serial.print("Humidity (%): ");
        Serial.println(humidity);

        if (CO2 > 0)
        {
            sensorData[0].isValid = true;
            sensorData[0].value = CO2;
            sensorData[0].unit = "ppm";
            sensorData[0].name = "CO2";
            if (CO2 < 400)
            {
                double offset = 420 - CO2;
                airSensor.setForcedRecalibrationFactor(400 + offset);
                Serial.print("Prevent values below 400. Increase concentration to: ");
                Serial.println(400 + offset);
                sensorData[0].value = 400;
            }
        }
        if (temperature > 0)
        {
            sensorData[1].isValid = true;
            sensorData[1].value = temperature;
            sensorData[1].unit = "*C";
            sensorData[1].name = "Temperature";
        }
        if (humidity > 0)
        {
            sensorData[2].isValid = true;
            sensorData[2].value = humidity;
            sensorData[2].unit = "%";
            sensorData[2].name = "Humidity";
        }

        return sensorData;
    }

    std::array<SensorData, 3> getWaterValues()
    {
        std::array<SensorData, 3> sensorData;
        sensorData.fill(SensorData());

        float clicks = watersensor::getClicks();
        Serial.print("Clicks: ");
        Serial.println(clicks);
        sensorData[0].isValid = true;
        sensorData[0].value = clicks;
        sensorData[0].unit = "1";
        sensorData[0].name = "WaterClicks";

        return sensorData;
    }

    std::array<SensorData, 3> getWindValues()
    {
        std::array<SensorData, 3> sensorData;
        sensorData.fill(SensorData());

        float clicks = windsensor::getClicks();
        Serial.print("Clicks: ");
        Serial.println(clicks);
        sensorData[0].isValid = true;
        sensorData[0].value = clicks;
        sensorData[0].unit = "1";
        sensorData[0].name = "WindClicks";

        std::pair<double, double> speed = windsensor::getSpeed();
        Serial.print("Speed: ");
        Serial.println(speed.first, 9);
        sensorData[1].isValid = true;
        sensorData[1].value = speed.first;
        sensorData[1].unit = "m/s";
        sensorData[1].name = "WindSpeed";

        double peak = speed.second;
        Serial.print("Peak: ");
        Serial.println(peak, 9);
        sensorData[2].isValid = true;
        sensorData[2].value = peak;
        sensorData[2].unit = "m/s";
        sensorData[2].name = "WindPeak";

        return sensorData;
    }

    std::array<SensorData, 3> getDS18B20Values()
    {
        std::array<SensorData, 3> sensorData;
        sensorData.fill(SensorData());

        auto temps = one_wire::getTemperatures();
        int counter = 0;
        for (auto temp : temps)
        {
            if (counter > 2)
            {
                Serial.printf("Only 3 allowed");
                break;
            }
            Serial.print("Temperature: ");
            Serial.println(temp);
            sensorData[counter].isValid = true;
            sensorData[counter].value = temp;
            sensorData[counter].unit = "*C";
            sensorData[counter].name = "Temperature" + String(counter);
            counter++;
        }
        return sensorData;
    }

    void initI2CSensor(uint8_t address)
    {
        Serial.print("Init Sensor: ");
        Serial.println(address);
        delay(1.0);
        if (address == 0x76)
        {
#if USE_ALL_SENSORS
            unsigned status;
            status = bme.begin(address, &MyWire);
            if (!status)
            {
                Serial.print("SensorID was: 0x");
                Serial.println(bme.sensorID(), 16);

                if (!bmp.begin(0x76))
                {
                    Serial.println(F("Could not find a valid BMP280 sensor, check wiring or "
                                     "try a different address!"));
                    return;
                }
                bmp.setSampling(Adafruit_BMP280::MODE_NORMAL,     /* Operating Mode. */
                                Adafruit_BMP280::SAMPLING_X2,     /* Temp. oversampling */
                                Adafruit_BMP280::SAMPLING_X16,    /* Pressure oversampling */
                                Adafruit_BMP280::FILTER_X16,      /* Filtering. */
                                Adafruit_BMP280::STANDBY_MS_500); /* Standby time. */
                bmp_temp->printSensorDetails();
                m_SensorTypes.insert(SensorType::bmp280);
                m_Description = m_Description + "BMP280;";
            }
            else
            {
                m_SensorTypes.insert(SensorType::bme280);
                m_Description = m_Description + "BME280;";
            }

        }
        else if (address == 0x44)
        {
            Serial.println("Sensor SHT30.");
            m_SensorTypes.insert(SensorType::sht30);
            m_Description = m_Description + "SHT30;";

#endif
        }
        else if (address == 0x61)
        {
            Serial.println("Sensor SCD30.");
            initSCD30();
        }
        else if (address == 0x58)
        {
            /*Serial.println("Sensor SGP30.");
        Serial.print("Init SGP30 CO2 / TVOC");
        m_SensorTypes["sgp30"] = SensorType::sgp30;
        if (!sgp.begin(&MyWire))
        {
            Serial.println("Init SGP30 Failed");
        };
        Serial.print(sgp.serialnumber[0], HEX);
        Serial.print(sgp.serialnumber[1], HEX);
        Serial.println(sgp.serialnumber[2], HEX);*/
        }
    }

    void initSCD30()
    {
        if (airSensor.begin(MyWire, false) && airSensor.isConnected())
        {
            Serial.println("Found SCD30 (CO2)");

            m_SensorTypes.insert(SensorType::scd30);
            m_Description = m_Description + "SCD30;";
            airSensor.setForcedRecalibrationFactor(420); // Assuming outdoor conditions.
            delay(300);

            unsigned long timeoutTime = millis() + 5000;
            while (!airSensor.dataAvailable())
            {
                delay(200);
                if (millis() > timeoutTime)
                {
                    Serial.println("Timeout: No data available.");
                }
            }

            double co2 = airSensor.getCO2();
            Serial.print("Check calibration: ");
            Serial.println(co2);
            if (co2 < 400)
            {
                double offset = 420 - co2;
                airSensor.setForcedRecalibrationFactor(400 + offset);
                Serial.print("Prevent values below 400. Increase concentration to: ");
                Serial.println(400 + offset);
            }

            return;
        }
        Serial.println("SCD sensor not detected.");
        uint16_t fwVer;
        airSensor.getFirmwareVersion(&fwVer);
        Serial.println(fwVer, HEX);

        delay(300);
    }

    const String &getDescription()
    {
        Serial.println(m_Description);
        return m_Description;
    }
#if DISABLE_SCD40
    bool initSCD40()
    {

        uint16_t error;
        char errorMessage[256];

        scd4x.begin(MyWire);

        // stop potentially previously started measurement
        error = scd4x.stopPeriodicMeasurement();
        if (error)
        {
            Serial.print("Error trying to execute stopPeriodicMeasurement(): ");
            errorToString(error, errorMessage, 256);
            Serial.println(errorMessage);
            return false;
        }

        uint16_t serial0;
        uint16_t serial1;
        uint16_t serial2;
        error = scd4x.getSerialNumber(serial0, serial1, serial2);
        if (error)
        {
            Serial.print("Error trying to execute getSerialNumber(): ");
            errorToString(error, errorMessage, 256);
            Serial.println(errorMessage);
            return false;
        }
        scd40printSerialNumber(serial0, serial1, serial2);

        Serial.println("Found SCD40 (CO2)");

        error = scd4x.startPeriodicMeasurement();
        if (error)
        {
            Serial.print("Error trying to execute startPeriodicMeasurement(): ");
            errorToString(error, errorMessage, 256);
            Serial.println(errorMessage);
            return false;
        }

        m_SensorTypes.insert(SensorType::scd40);
        m_Description = m_Description + "SCD40;";
        return true;
    }

    void scd40printUint16Hex(uint16_t value)
    {
        Serial.print(value < 4096 ? "0" : "");
        Serial.print(value < 256 ? "0" : "");
        Serial.print(value < 16 ? "0" : "");
        Serial.print(value, HEX);
    }

    void scd40printSerialNumber(uint16_t serial0, uint16_t serial1, uint16_t serial2)
    {
        Serial.print("Serial: 0x");
        scd40printUint16Hex(serial0);
        scd40printUint16Hex(serial1);
        scd40printUint16Hex(serial2);
        Serial.println();
    }
#endif

}; // namespace sensor