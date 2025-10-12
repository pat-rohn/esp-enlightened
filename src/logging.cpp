

#include <ArduinoJson.h>
#include <logging.h>

#ifdef ESP8266
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#endif /* ESP8266 */

#ifdef ESP32
#include "WiFi.h"
#include <HTTPClient.h>
#endif /* ESP32 */

namespace logging
{
    CLogger::CLogger(const String &serverAddress, const String &deviceID) : m_ServerAddress(serverAddress),
                                                                            m_DeviceID(deviceID)
    {
        Serial.printf("[INFO] Server address %s\n", m_ServerAddress.c_str());
    }

    void CLogger::logMessage(const String &msg)
    {
        if (!m_IsOnline)
        {
            Serial.printf("[OFFLINE] %s: %s\n", m_DeviceID.c_str(), msg.c_str());
            return;
        }
        Serial.printf("[LOG] %s: %s\n", m_DeviceID.c_str(), msg.c_str());

        try
        {
            JsonDocument doc; // Fixed size for better memory management
            auto tsEntry = doc.to<JsonObject>();

            tsEntry["Device"] = m_DeviceID;
            tsEntry["Text"] = msg;
            tsEntry["Level"] = 2;

            String jsonString;
            serializeJson(doc, jsonString);

            if (postData(jsonString, "/api/log"))
            {
                Serial.printf("[SUCCESS] %s: %s\n", m_DeviceID.c_str(), msg.c_str());
            }
            else
            {
                Serial.printf("[FAILED] %s: %s\n", m_DeviceID.c_str(), msg.c_str());
            }
        }
        catch (const std::exception &e)
        {
            Serial.printf("[ERROR] Logging failed: %s\n", e.what());
        }
    }

    bool CLogger::postData(const String &root, const String &url)
    {
        WiFiClient client = WiFiClient();
        HTTPClient http;
        String serverPath = "http://" + m_ServerAddress + url;
        Serial.printf("[LOG] Send message from %s to %s\n", m_DeviceID.c_str(), serverPath.c_str());

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
        return true;
    }
}