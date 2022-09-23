#include "sensors.h"
//#include <M5Core2.h>
#include <Adafruit_Sensor.h>

#include "DHT.h"
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_BME280.h>
#include "Adafruit_CCS811.h"
//#include "Adafruit_SGP30.h"
//#include <WEMOS_SHT3X.h>
#include <Adafruit_BMP280.h>
#include <MHZ19.h>
#include <SparkFun_SCD30_Arduino_Library.h>
#include "sensors/watersensor.h"
#include "sensors/windsensor.h"

#ifdef ESP8266
#include <SoftwareSerial.h>
int dhTPin = -1;
// Digital Input: D1,D2,D5,D6,D7
int waterSensorPin = -1;
int windSensorPin = D5;
#else
int dhTPin = -1;
// Digital Input: e.g. D16-D33
int waterSensorPin = -1;
int windSensorPin = -1;
#endif /* ESP8266 */

namespace sensor
{

    std::vector<SensorType> m_SensorTypes;
    std::vector<String> m_ValueNames;
    String m_Description = "";

    Adafruit_BME280 bme; // I2C

    Adafruit_CCS811 ccs;
    TwoWire MyWire = Wire;
    SCD30 airSensor;

    Adafruit_BMP280 bmp = Adafruit_BMP280(&MyWire);
    Adafruit_Sensor *bmp_temp = bmp.getTemperatureSensor();
    Adafruit_Sensor *bmp_pressure = bmp.getPressureSensor();
    // SHT3X sht30(0x44);
    DHTSensor *dhtSensor;

    MHZ19 myMHZ19;

    // Adafruit_SGP30 sgp;

#ifdef ESP32
    HardwareSerial MySerial = Serial2;
#else
    SoftwareSerial MySerial(D7, D6);
#endif

    bool sensorsInit()
    {
        Serial.println("Sensors init.");

        m_SensorTypes.clear();
        if (dhTPin >= 0)
        {
            dhtSensor = new DHTSensor(D3);

            if (dhtSensor->init())
            {
                m_SensorTypes.emplace_back(SensorType::dht22);
                m_Description = m_Description + "DHT22;";
                m_ValueNames.emplace_back("Temperature");
                m_ValueNames.emplace_back("Humidity");
            }
        }

#ifdef ESP32
        MySerial.begin(9600, SERIAL_8N1, RX, TX);
#else
        MySerial.begin(9600);
#endif
        findAndInitSensors();
        if (waterSensorPin >= 0)
        {
            m_SensorTypes.emplace_back(SensorType::watersensor);
            watersensor::start(waterSensorPin);
        }
        if (windSensorPin >= 0)
        {
            m_SensorTypes.emplace_back(SensorType::windsensor);
            windsensor::start(windSensorPin);
        }

        if (m_SensorTypes.empty())
        {
            // MyWire.begin(32, 33);
            Serial.println("Change Wire since nothing found.");
            return false;
        }
        return true;
    }

    void findAndInitSensors()
    {
        Serial.println("Find and init i2c sensors");
        byte count = 0;
        MyWire.begin(SDA, SCL);
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

        Serial.println();

        findAndInitMHZ19();
    }

    void findAndInitMHZ19()
    {
        Serial.println("Find and init MHZ19 sensor");

        myMHZ19.begin(MySerial);
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
            m_SensorTypes.emplace_back(SensorType::mhz19);
            m_ValueNames.emplace_back("CO2");
            m_Description = m_Description + "MHZ19(" + v + ");";
            myMHZ19.autoCalibration();
            return;
        }
    }

    std::map<String, SensorData> getValues()
    {
        if (m_SensorTypes.empty())
        {
            Serial.println("No Sensors detected?");
        }
        std::map<String, SensorData> res;

        if (std::find(m_SensorTypes.begin(), m_SensorTypes.end(), SensorType::dht22) != m_SensorTypes.end())
        {
            for (const auto &val : getDHT22())
            {
                if (val.isValid)
                {
                    res[val.name] = val;
                }
            }
        }

        if (std::find(m_SensorTypes.begin(), m_SensorTypes.end(), SensorType::cjmcu) != m_SensorTypes.end())
        {
            for (const auto &val : getCjmcu())
            {
                if (val.isValid)
                {
                    res[val.name] = val;
                }
            }
        }
        if (std::find(m_SensorTypes.begin(), m_SensorTypes.end(), SensorType::bme280) != m_SensorTypes.end())
        {
            for (const auto &val : getBME280())
            {
                if (val.isValid)
                {
                    res[val.name] = val;
                }
            }
        }
        if (std::find(m_SensorTypes.begin(), m_SensorTypes.end(), SensorType::bmp280) != m_SensorTypes.end())
        {
            for (const auto &val : getEnv())
            {
                if (val.isValid)
                {
                    res[val.name] = val;
                }
            }
        }

        if (std::find(m_SensorTypes.begin(), m_SensorTypes.end(), SensorType::mhz19) != m_SensorTypes.end())
        {
            for (const auto &val : getMHZ19())
            {
                if (val.isValid)
                {
                    res[val.name] = val;
                }
            }
        }

        if (std::find(m_SensorTypes.begin(), m_SensorTypes.end(), SensorType::scd30) != m_SensorTypes.end())
        {
            for (const auto &val : getSCD30())
            {
                if (val.isValid)
                {
                    res[val.name] = val;
                }
            }
        }

        if (std::find(m_SensorTypes.begin(), m_SensorTypes.end(), SensorType::watersensor) != m_SensorTypes.end())
        {
            for (const auto &val : getWaterValues())
            {
                if (val.isValid)
                {
                    res[val.name] = val;
                }
            }
        }
        if (std::find(m_SensorTypes.begin(), m_SensorTypes.end(), SensorType::windsensor) != m_SensorTypes.end())
        {
            for (const auto &val : getWindValues())
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

    std::array<SensorData, 3> getCjmcu()
    {
        std::array<SensorData, 3> sensorData;
        sensorData.fill(SensorData());

        if (ccs.available())
        {
            while (!ccs.available())
            {
                delay(0.1);
            }
            auto res = ccs.readData();
            for (int i = 0; i < 5; i++)
            {
                if (res == 0)
                {
                    break;
                }
                res = ccs.readData();
                delay(0.3);
            }
            if (!res)
            {
                sensorData[0].isValid = true;
                sensorData[0].value = ccs.geteCO2();
                sensorData[0].unit = "ppm";
                sensorData[0].name = "CO2";

                sensorData[1].isValid = true;
                sensorData[1].value = ccs.getTVOC();
                sensorData[1].unit = "ppb";
                sensorData[1].name = "TVOC";

                Serial.print("CO2: ");
                Serial.println(sensorData[0].value);
                Serial.print("TVOC: ");
                Serial.println(sensorData[1].value);
            }
            else
            {
                Serial.println("cjmcu ERROR");
                return sensorData;
            }
        }
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
        double vol = watersensor::getValue();
        Serial.print("Volume: ");
        Serial.println(vol, 9);
        Serial.println("mm");
        sensorData[0].isValid = true;
        sensorData[0].value = vol;
        sensorData[0].unit = "mm";
        sensorData[0].name = "WaterVolume";

        float clicks = watersensor::getClicks();
        Serial.print("Clicks: ");
        Serial.println(clicks);
        sensorData[1].isValid = true;
        sensorData[1].value = clicks;
        sensorData[1].unit = "1";
        sensorData[1].name = "WaterClicks";

        double flow = watersensor::getFlow();
        Serial.print("Flow: ");
        Serial.println(flow, 9);
        sensorData[2].isValid = true;
        sensorData[2].value = flow;
        sensorData[2].unit = "mm/s";
        sensorData[2].name = "WaterFlow";

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

        double flow = windsensor::getSpeed();
        Serial.print("Speed: ");
        Serial.println(flow, 9);
        sensorData[1].isValid = true;
        sensorData[1].value = flow;
        sensorData[1].unit = "m/s";
        sensorData[1].name = "WindSpeed";

        return sensorData;
    }

    void initI2CSensor(uint8_t address)
    {
        Serial.print("Init Sensor: ");
        Serial.println(address);
        delay(1.0);
        if (address == 0x76)
        {
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
                m_SensorTypes.emplace_back(SensorType::bmp280);
                m_Description = m_Description + "BMP280;";

                m_ValueNames.emplace_back("Temperature");
                m_ValueNames.emplace_back("Humidity");
                m_ValueNames.emplace_back("Pressure");
            }
            else
            {
                m_SensorTypes.emplace_back(SensorType::bme280);
                m_Description = m_Description + "BME280;";
                m_ValueNames.emplace_back("Temperature");
                m_ValueNames.emplace_back("Humidity");
            }
        }
        else if (address == 0x5A)
        {
            Serial.println("Init Sensor cjmcu");
            if (!ccs.begin())
            {
                Serial.println("Failed to start sensor! Please check your wiring.");
                return;
            }

            Serial.println("cjmcu: wait till available");
            // calibrate temperature sensor
            while (!ccs.available())
            {
                delay(10);
            }
            Serial.println("cjmcu: Calc temperature");
            float cTemp = ccs.calculateTemperature();
            ccs.setTempOffset(cTemp - 25.0);
            getCjmcu();
            Serial.println("cjmcu: Add to sensors");
            m_SensorTypes.emplace_back(SensorType::cjmcu);
            m_ValueNames.emplace_back("CO2");
            m_ValueNames.emplace_back("TVOC");
            m_Description = m_Description + "CJMCU;";
        }
        else if (address == 0x44)
        {
            Serial.println("Sensor SHT30.");
            m_SensorTypes.emplace_back(SensorType::sht30);
            m_Description = m_Description + "SHT30;";
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

            m_SensorTypes.emplace_back(SensorType::scd30);
            m_Description = m_Description + "SCD30;";
            m_ValueNames.emplace_back("CO2");
            m_ValueNames.emplace_back("Temperature");
            m_ValueNames.emplace_back("Humidity");
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

    std::vector<String> getValueNames()
    {
        for (auto &s : m_ValueNames)
        {
            Serial.println(s);
        }

        return m_ValueNames;
    }

    const String &getDescription()
    {
        Serial.println(m_Description);
        return m_Description;
    }

}; // namespace sensor