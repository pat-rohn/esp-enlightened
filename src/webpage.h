

#ifndef WEBPAGE_H
#define WEBPAGE_H

#include <Arduino.h>
#include "timehelper.h"
#include "led/leds_service.h"
#include <ESPAsyncWebServer.h>

#ifdef ESP32
#include <WiFi.h>
#include <AsyncTCP.h>
#else
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
#endif


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
     
    public:
        static void setLEDService(CLEDService *ledService);
        static void setTimeHelper(CTimeHelper *timeHelper);

        static String processor(const String &var);
    };

}
#endif