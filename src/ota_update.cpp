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
        ArduinoOTA.setHostname(configuration.SensorID.c_str());

        // OTA stays available without an ApiToken. Gating it on the token made
        // the token unrecoverable: a stray or forgotten token locks every
        // mutating endpoint, and clearing it used to take OTA down with it,
        // leaving USB as the only way back in.
        //
        // The trade-off is deliberate and it is real: with no token set, any
        // host that can reach this device on the network can flash arbitrary
        // firmware onto it. Set an ApiToken to require a password.
        if (configuration.ApiToken.length() > 0)
        {
            ArduinoOTA.setPassword(configuration.ApiToken.c_str());
            Serial.println("OTA enabled (password protected)");
        }
        else
        {
            Serial.println("OTA enabled WITHOUT a password: any host on this "
                           "network can flash this device. Set an ApiToken to "
                           "require one.");
        }

        ArduinoOTA.begin();
        enabled = true;
    }

    void handle()
    {
        if (enabled)
        {
            ArduinoOTA.handle();
        }
    }
}
