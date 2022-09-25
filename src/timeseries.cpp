#ifdef ESP8266
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#endif /* ESP8266 */

#ifdef ESP32
#include "WiFi.h"
#include <HTTPClient.h>
#endif /* ESP32 */

#include <ArduinoJson.h>
#include "timeseries.h"

void CTimeseriesData::addValue(const double &value, String timestamp)
{
    if (!timestamp.isEmpty())
    {
        m_DataSeries.emplace_back(DataPoint(timestamp, value));
    }
}

CTimeseries::CTimeseries(const String &timeseriesAddress, const String &port)
{
    m_ServerAddress = "http://";
    m_ServerAddress += timeseriesAddress;
    m_ServerAddress += ":";
    m_ServerAddress += port;
    m_TimeHelper = CTimeHelper();
}

CTimeseries::Device CTimeseries::init(const DeviceDesc &deviceDesc)
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
    String serverPath = m_ServerAddress;
    serverPath += "/init-device";
    Serial.println(serverPath);

    http.begin(client, serverPath.c_str());
    Serial.print("Post deviceDesc: ");
    Serial.println(deviceDesc.Description);

    int httpResponseCode = http.POST(doc.as<String>());

    if (httpResponseCode > 0)
    {
        Serial.print("HTTP Response code: ");
        Serial.println(httpResponseCode);
        const char *payload = http.getString().c_str();
        // Serial.println(payload);
        http.end();
        return deserializeDevice(payload);
    }
    else
    {
        Serial.print("Error code: ");
        Serial.println(httpResponseCode);
        http.end();
        delay(5000);
        return init(deviceDesc);
    }
    return Device("No Device", 60.0, 3);
}

CTimeseries::Device CTimeseries::deserializeDevice(const char *deviceJson)
{
    StaticJsonDocument<4000> doc;
    DeserializationError error = deserializeJson(doc, deviceJson);

    if (error)
    {
        Serial.print("deserializeJson() failed: ");
        Serial.println(error.c_str());
        return CTimeseries::Device("No Device", 60.0, 3);
    }

    const char *Name = doc["Name"]; // "myName"
    int Interval = doc["Interval"]; // 60.0
    int Buffer = doc["Buffer"];     // 3
    Device device(Name, Interval, Buffer);
    Serial.print("Device: ");
    Serial.println(device.Name);
    Serial.print("Interval/Buffer: ");
    Serial.print(device.Interval);
    Serial.print("/");
    Serial.println(device.Buffer);
    Serial.println("Sensors: ");
    device.Sensors = std::vector<Sensor>();
    for (JsonObject JSONSensor : doc["Sensors"].as<JsonArray>())
    {
        const char *Sensor_Name = JSONSensor["Name"];
        double Sensor_Offset = JSONSensor["Offset"];
        CTimeseries::Sensor sensor = Sensor();
        sensor.Name = Sensor_Name;
        sensor.Offset = Sensor_Offset;
        Serial.print(sensor.Name);
        Serial.print(" Offset:  ");
        Serial.print(sensor.Offset);
        Serial.println("");
        device.Sensors.emplace_back(sensor);
    }

    return device;
}

void CTimeseries::addValue(const String &name, const double &value)
{
    if (m_Data.find(name) == m_Data.end())
    {
        m_Data.insert(std::pair<String, CTimeseriesData>(name, CTimeseriesData(name)));
    }
    m_Data.at(name).addValue(value, m_TimeHelper.getTimestamp());
}

bool CTimeseries::sendData()
{
    // https://arduinojson.org/v6/assistant/
    // https://arduinojson.org/v6/how-to/determine-the-capacity-of-the-jsondocument/

    DynamicJsonDocument doc(10000); // uses heap because it's too much data for stack
    for (auto const &ts : m_Data)
    {
        JsonObject tsEntry = doc.createNestedObject();
        tsEntry["Tag"] = ts.first;
        JsonArray tsValuesTS = tsEntry.createNestedArray("Timestamps");
        JsonArray tsValuesV = tsEntry.createNestedArray("Values");
        for (auto val : ts.second.m_DataSeries)
        {
            tsValuesTS.add(val.Timestamp);
            if (val.Value < 0.00001)
            {
                tsValuesV.add(String(val.Value, 8));
            }else if (val.Value < 0.001)
            {
                tsValuesV.add(String(val.Value, 5));
            }
            else 
            {
                tsValuesV.add(String(val.Value, 4));
            }
        }
    }
    Serial.println(doc.as<String>());
    if (postData(doc.as<String>(), "/timeseries/save"))
    {
        m_Data.clear();
        return true;
    }
    return false;
}

bool CTimeseries::postData(const String &root, const String &url)
{
    WiFiClient client = WiFiClient();
    HTTPClient http;
    String serverPath = m_ServerAddress;
    serverPath += url;
    Serial.println(serverPath);

    http.begin(client, serverPath.c_str());
    Serial.println("Post data");
    int httpResponseCode = http.POST(root.c_str());

    if (httpResponseCode > 0)
    {
        Serial.print("HTTP Response code: ");
        Serial.println(httpResponseCode);
        String payload = http.getString();
        Serial.println(payload);
        http.end();
        return true;
    }
    else
    {
        Serial.print("Error code: ");
        Serial.println(httpResponseCode);
        http.end();
        return false;
    }
    // Free resources
    return true;
}
