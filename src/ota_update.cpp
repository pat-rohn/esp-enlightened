#include "ota_update.h"

#include <ArduinoOTA.h>

namespace ota_update
{
    namespace
    {
        bool enabled = false;
    }

    void begin(const configman::Configuration &configuration)
    {
        if (configuration.ApiToken.length() == 0)
        {
            Serial.println("OTA disabled: configure ApiToken first");
            return;
        }

        ArduinoOTA.setHostname(configuration.SensorID.c_str());
        ArduinoOTA.setPassword(configuration.ApiToken.c_str());
        ArduinoOTA.begin();
        enabled = true;
        Serial.println("OTA enabled");
    }

    void handle()
    {
        if (enabled)
        {
            ArduinoOTA.handle();
        }
    }
}
