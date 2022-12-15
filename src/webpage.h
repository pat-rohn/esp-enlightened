

#ifndef WEBPAGE_H
#define WEBPAGE_H

#include <Arduino.h>
#include "timehelper.h"
#include "led/leds_service.h"

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

    public:
        void beginServer();
     
        static void notFound(AsyncWebServerRequest *request)
        {
            request->send(404, "text/plain", "Not found");
        }

    public:
        static void setLEDService(CLEDService *ledService);

        static String processor(const String &var);
    };

}
#endif