
#include <OneWire.h>
#include <DallasTemperature.h>

namespace one_wire
{

    OneWire oneWire;

    DallasTemperature sensors;
    int numberOfDevices;
    DeviceAddress tempDeviceAddress;

    bool init(int pin)
    {
        Serial.printf("ONE WIRE: Init devices on pin %d\n", pin);
        oneWire = OneWire(pin);
        sensors = DallasTemperature(&oneWire);
        sensors.begin();
        numberOfDevices = sensors.getDeviceCount();
        if (numberOfDevices > 0)
        {

            Serial.print("ONE WIRE: Locating devices...");
            Serial.print("Found ");
            Serial.print(numberOfDevices, DEC);
            Serial.println(" devices.");
            return true;
        }
        return true;
    }

    double getTemperature()
    {
        Serial.print("Read temperature of One Wire ... \n ");
        sensors.requestTemperatures();
        for (int i = 0; i < numberOfDevices; i++)
        {
            // Search the wire for address
            if (sensors.getAddress(tempDeviceAddress, i))
            {

                // Output the device ID
                Serial.print("Temperature for device: ");
                Serial.println(i, DEC);

                // Print the data
                float tempC = sensors.getTempC(tempDeviceAddress);
                Serial.print("Temp C: ");
                Serial.print(tempC);
                return tempC;
            }
        }
        return 0;
    }
};
