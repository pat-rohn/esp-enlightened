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

CTimeHelper::Device CTimeseries::init(const String &name, const std::vector<String> &sensors)
{
    StaticJsonDocument<192> doc;

    JsonObject Device = doc.createNestedObject("Device");
    Device["Name"] = name;

    JsonArray Device_Sensors = Device.createNestedArray("Sensors");
    for (const String &s : sensors)
    {
        Device_Sensors.add(name + s);
    }

    WiFiClient client = WiFiClient();
    HTTPClient http;
    String serverPath = m_ServerAddress;
    serverPath += "/init-device";
    Serial.println(serverPath);

    http.begin(client, serverPath.c_str());
    Serial.println("Post data");

    int httpResponseCode = http.POST(doc.as<String>());

    if (httpResponseCode > 0)
    {
        Serial.print("HTTP Response code: ");
        Serial.println(httpResponseCode);
        const char * payload = http.getString().c_str();
        //Serial.println(payload);
        http.end();
        return deserializeDevice(payload);
    }
    else
    {
        Serial.print("Error code: ");
        Serial.println(httpResponseCode);
        http.end();
        delay(5000);
        init(name, sensors);
    }
    return CTimeHelper::Device("No Device", 60.0, 3);
}

CTimeHelper::Device CTimeseries::deserializeDevice(const char *deviceJson)
{
    StaticJsonDocument<4000> doc;
    DeserializationError error = deserializeJson(doc, deviceJson);

    if (error)
    {
        Serial.print("deserializeJson() failed: ");
        Serial.println(error.c_str());
        return CTimeHelper::Device("No Device", 60.0, 3);
    }

    const char *Device = doc["Name"];                      // "myName"
    int Interval = doc["Interval"]; // 60.0
    int Buffer = doc["Buffer"]; // 3
    CTimeHelper::Device device(Device, Interval, Buffer);
    Serial.print("Device: ");
    Serial.println(device.Name);
    Serial.print("Interval/Buffer: ");
    Serial.print(device.Interval);
    Serial.print("/");
    Serial.println(device.Buffer);
    Serial.println("Sensors: ");
    device.Sensors = std::vector<CTimeHelper::Sensor>();
    for (JsonObject Sensor : doc["Sensors"].as<JsonArray>())
    {
        const char *Sensor_Name = Sensor["Name"]; // "askdmlaksmdasmySensor1", "askdmlaksmdasmySensor2", ...
        double Sensor_Offset = Sensor["Offset"];  // 1.2, 1.2, 1.2
        CTimeHelper::Sensor sensor = CTimeHelper::Sensor();
        sensor.Name = Sensor_Name;
        sensor.Offset = Sensor_Offset;
        Serial.print(sensor.Name);
        Serial.print("  ");
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
    //https://arduinojson.org/v6/assistant/
    //https://arduinojson.org/v6/how-to/determine-the-capacity-of-the-jsondocument/

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
            tsValuesV.add(String(val.Value));
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
