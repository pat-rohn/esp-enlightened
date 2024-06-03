

#ifndef WEBPAGE_H
#define WEBPAGE_H

#include <Arduino.h>
#include "timehelper.h"
#include "led/leds_service.h"
#include <ESPAsyncWebServer.h>
#include <atomic>
#include <memory>

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
        static void setTriggerFlag(std::atomic<bool> *restartTriggered);
        static void setButtonsPressed(std::atomic<bool> *buttonPressed1, std::atomic<bool> *buttonPressed2);
        static String processor(const String &var);
    };

}
#endif