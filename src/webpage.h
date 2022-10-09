

#ifndef SUNRISE_H
#define SUNRISE_H

#include <Arduino.h>
#include "timehelper.h"

#ifdef ESP32
#include <WiFi.h>
#include <AsyncTCP.h>
#else
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#endif

#include <ESPAsyncWebServer.h>

#ifdef ESP8266
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#endif /* ESP8266 */

#include "config.h"

namespace webpage
{

    class CWebPage
    {
    public:
        CWebPage();
        ~CWebPage(){};

    public:
        AsyncWebServer m_Server;
        String m_Header;

        CTimeHelper *m_TimeHelper;
        void beginServer();
        String getFrontPage();
        String getHTTPOK();
        String getHTTPNotOK();
        void showError();

    public:
        static void notFound(AsyncWebServerRequest *request)
        {
            request->send(404, "text/plain", "Not found");
        }

        static String processor(const String &var)
        {
            Serial.println(var);
            if (var == configman::kPathToConfig)
            {
                Serial.println("read from configuration");
                return configman::readConfigAsString();
            }
            else
            {
                Serial.println("there is nothing yet with the call " + var);
                Serial.println(configman::kPathToConfig);
                return "there is nothing yet with the call" + var;
            }
            return String();
        }

    private:
        unsigned long m_CurrentTime;
        unsigned long m_PreviousTime;
        unsigned long m_TimeoutTime;
    };

}
#endif