

#include "timeseries.h"
#include <ArduinoJson.h>

#ifdef ESP8266
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#endif /* ESP8266 */

#ifdef ESP32
#include "WiFi.h"
#include <HTTPClient.h>
#endif /* ESP32 */

namespace timeseries
{

    Device CTimeseries::init(const DeviceDesc &deviceDesc)
    {
        StaticJsonDocument<300> doc;

        JsonObject JSONDevice = doc.createNestedObject("Device");
        JSONDevice["Name"] = deviceDesc.Name;
        JSONDevice["Description"] = deviceDesc.Description;

        JsonArray Device_Sensors = JSONDevice.createNestedArray("Sensors");
        for (const String &s : deviceDesc.Sensors)
        {
            Device_Sensors.add(deviceDesc.Name + s);
        }

        WiFiClient client = WiFiClient();
        HTTPClient http;
        String serverPath = "http://"+m_ServerAddress;
        serverPath += "/init-device";
        Serial.println(serverPath);

        http.begin(client, serverPath.c_str());
        Serial.print("Post deviceDesc: ");
        Serial.println(deviceDesc.Description);

        for (int i = 0; i <= 3; i++)
        {
            int httpResponseCode = http.POST(doc.as<String>());

            if (httpResponseCode > 0)
            {

                auto device = deserializeDevice(http.getString());
                http.end();
                return device;
            }
            else
            {
                Serial.printf("Error code: %d\n", httpResponseCode);
                http.end();
                delay(5000);
            }
        }

        return Device("No Device", 60.0, 3);
    }

    Device CTimeseries::deserializeDevice(const String &deviceJson)
    {
        Serial.print("deserialize device");
        StaticJsonDocument<4000> doc;
        DeserializationError error = deserializeJson(doc, deviceJson);
        if (error)
        {
            Serial.print("deserializeJson() failed: ");
            Serial.println(error.c_str());
            return Device("No Device", 60.0, 3);
        }

        const char *Name = doc["Name"]; // "myName"
        int Interval = doc["Interval"]; // 60.0
        int Buffer = doc["Buffer"];     // 3
        Device device(Name, Interval, Buffer);
        Serial.printf("Device: %s (interval/buffer: %f / %d ", device.Name.c_str(), device.Interval, device.Buffer);

        Serial.println("Sensors: ");
        device.Sensors = std::vector<Sensor>();
        for (JsonObject JSONSensor : doc["Sensors"].as<JsonArray>())
        {
            const char *Sensor_Name = JSONSensor["Name"];
            double Sensor_Offset = JSONSensor["Offset"];
            Sensor sensor = Sensor();
            sensor.Name = Sensor_Name;
            sensor.Offset = Sensor_Offset;
            Serial.printf("%s with offset %f \n", sensor.Name.c_str(), sensor.Offset);

            device.Sensors.emplace_back(sensor);
        }

        return device;
    }

}