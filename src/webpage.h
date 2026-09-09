

#ifndef WEBPAGE_H
#define WEBPAGE_H

#include <Arduino.h>
#include "timehelper.h"
#include "led/leds_service.h"
#include "led/sunrise_alarm.h"
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
    // Bumped on every breaking change to the HTTP contract. Reported in
    // /api/status so a client on the wrong release can say so in words rather
    // than surfacing a bare 404 from a route that no longer exists.
    //
    // 2: uniform token gate, partial writes, /api/status carries light and
    //    sensors, /api/version + /api/time + GET /api/led removed.
    constexpr int kApiVersion = 2;


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

    private:
        void registerConfigRoutes();
        void registerLedRoutes();
        void registerCommandRoutes();
        void registerStatusRoutes();
        void registerOptionsRoutes();

    public:
        static void setLEDService(CLEDService *ledService);
        static void setTimeHelper(CTimeHelper *timeHelper);
        static void setSunriseAlarm(sunrise::CSunriseAlarm *sunriseAlarm);
        static void setTriggerFlag(std::atomic<bool> *restartTriggered);
        static void setButtonsPressed(std::atomic<bool> *buttonPressed1, std::atomic<bool> *buttonPressed2);
    };

}
#endif